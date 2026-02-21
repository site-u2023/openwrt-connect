<#
.SYNOPSIS
    Generate Product.wxs from .ini file

.DESCRIPTION
    Reads build settings from .ini file and generates
    a WiX installer definition with matching features and shortcuts.
    
    v2.0.0: Changed from .conf to .ini (build settings separated from runtime config)

.NOTES
    Called by build.bat before WiX compilation.
#>
param(
    [string]$IniFile     = "",
    [string]$OutputFile  = "Product.wxs",
    [string]$LicenseFile = "wix-eula.rtf"
)

# .ini自動検出: 未指定の場合はカレントディレクトリの最初の.iniを使用
if ($IniFile -eq "") {
    $found = Get-ChildItem -Path "." -Filter "*.ini" | Select-Object -First 1
    if ($null -eq $found) {
        Write-Error "No .ini file found in current directory."
        exit 1
    }
    $IniFile = $found.FullName
    Write-Host "Auto-detected ini: $IniFile"
}

$iniFileName = [System.IO.Path]::GetFileName($IniFile)

# ================================================== #
# Also detect .conf file for bundling into MSI       #
# ================================================== #
$confFile = Get-ChildItem -Path "." -Filter "*.conf" | Select-Object -First 1
if ($null -eq $confFile) {
    Write-Error "No .conf file found in current directory (required for MSI bundling)."
    exit 1
}
$confFileName = $confFile.Name
Write-Host "Auto-detected conf: $confFileName"

# ================================================== #
# Parse .ini file                                    #
# ================================================== #
function Parse-Ini {
    param([string]$Path)

    $general = @{
        product_name   = "OpenWrt Connect"
    }
    $commands = [System.Collections.ArrayList]::new()
    $currentSection = ""
    $currentCmd = $null

    foreach ($rawLine in Get-Content $Path -Encoding UTF8) {
        $line = $rawLine.Trim()
        if ($line -eq "" -or $line.StartsWith("#")) { continue }

        # Section header
        if ($line -match '^\[(.+)\]$') {
            $currentSection = $Matches[1]
            if ($currentSection -match '^command\.(.+)$') {
                $cmdName = $Matches[1]
                $currentCmd = @{
                    name  = $cmdName
                    label = ""
                    icon  = ""
                }
                [void]$commands.Add($currentCmd)
            } else {
                $currentCmd = $null
            }
            continue
        }

        # key = value
        if ($line -match '^([^=]+)=(.*)$') {
            $key = $Matches[1].Trim()
            $val = $Matches[2].Trim()

            if ($currentSection -eq "general") {
                $general[$key] = $val
            }
            elseif ($currentCmd) {
                $currentCmd[$key] = $val
            }
        }
    }

    return @{
        General  = $general
        Commands = $commands
    }
}

# ================================================== #
# Generate deterministic GUID from string            #
# ================================================== #
function Get-DeterministicGuid {
    param([string]$Seed)
    $md5 = [System.Security.Cryptography.MD5]::Create()
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($Seed)
    $hash = $md5.ComputeHash($bytes)
    $hash[6] = ($hash[6] -band 0x0F) -bor 0x30  # version 3
    $hash[8] = ($hash[8] -band 0x3F) -bor 0x80  # variant
    return [guid]::new($hash).ToString().ToUpper()
}

# ================================================== #
# Build WiX XML                                      #
# ================================================== #
# WiX ID sanitizer: hyphens to underscores
function Sanitize-Id { param([string]$s) return $s -replace '-', '_' }

$ini = Parse-Ini -Path $IniFile
$gen = $ini.General
$cmds = $ini.Commands

$xml = [System.Text.StringBuilder]::new()

function W { param([string]$s) [void]$xml.AppendLine($s) }

W '<?xml version="1.0" encoding="UTF-8"?>'
W '<Wix xmlns="http://schemas.microsoft.com/wix/2006/wi">'
W "  <Product Id=`"*`""
W "           Name=`"$($gen.product_name)`""
W '           Language="1041"'
W '           Codepage="65001"'
W '           Version="2.0.0.0"'
W '           Manufacturer="siteU"'
W '           UpgradeCode="9B606AC5-8CEF-47B9-B7D6-79787CADFFA5">'
W ''
W '    <Package InstallerVersion="200"'
W '             Compressed="yes"'
W '             InstallScope="perMachine" />'
W ''
W '    <MajorUpgrade DowngradeErrorMessage="A newer version is already installed." />'
W '    <MediaTemplate EmbedCab="yes" />'
W ''

# ================================================== #
# Icon declarations                                  #
# ================================================== #
$declaredIcons = @{}
foreach ($cmd in $cmds) {
    if ($cmd.icon -ne "" -and -not $declaredIcons.ContainsKey($cmd.icon)) {
        $iconId = Sanitize-Id ([System.IO.Path]::GetFileNameWithoutExtension($cmd.icon) + ".ico")
        W "    <Icon Id=`"$iconId`" SourceFile=`"$($cmd.icon)`" />"
        $declaredIcons[$cmd.icon] = $iconId
    }
}

# ARPPRODUCTICON - use main product icon
W "    <Icon Id=`"ProductIcon`" SourceFile=`"openwrt-connect.ico`" />"
W "    <Property Id=`"ARPPRODUCTICON`" Value=`"ProductIcon`" />"
W ''

# ================================================== #
# Feature tree                                       #
# ================================================== #
W '    <!-- Features -->'
W '    <Feature Id="FeatureMain" Title="Core" Description="Main executable and configuration" Level="1" Absent="disallow">'
W '      <ComponentRef Id="ExeComponent" />'
W '      <ComponentRef Id="ConfComponent" />'
W '    </Feature>'
W ''

foreach ($cmd in $cmds) {
    $featureId = "Feature_$($cmd.name)"
    $desc = if ($cmd.label -ne "") { $cmd.label } else { $cmd.name }
    W "    <Feature Id=`"$featureId`" Title=`"$($cmd.name)`" Description=`"$desc`" Level=`"1`">"
    W "      <ComponentRef Id=`"Shortcut_$($cmd.name)_StartMenu`" />"
    W "      <ComponentRef Id=`"Shortcut_$($cmd.name)_Desktop`" />"
    W '    </Feature>'
    W ''
}

# ================================================== #
# Directory structure                                #
# ================================================== #
W '    <!-- Directory structure -->'
W '    <Directory Id="TARGETDIR" Name="SourceDir">'
W '      <Directory Id="ProgramFilesFolder">'
W "        <Directory Id=`"INSTALLFOLDER`" Name=`"$($gen.product_name)`" />"
W '      </Directory>'
W '      <Directory Id="ProgramMenuFolder">'
W "        <Directory Id=`"ApplicationProgramsFolder`" Name=`"$($gen.product_name)`" />"
W '      </Directory>'
W '      <Directory Id="DesktopFolder" Name="Desktop" />'
W '    </Directory>'
W ''

# ================================================== #
# Main executable + conf                             #
# ================================================== #
W '    <!-- Main files -->'
W '    <DirectoryRef Id="INSTALLFOLDER">'
W "      <Component Id=`"ExeComponent`" Guid=`"C91DF506-205E-4C86-958F-3A4702ED257D`">"
W '        <File Id="openwrt_connect.exe" Source="openwrt-connect.exe" KeyPath="yes" />'
W '      </Component>'
$confFileId = Sanitize-Id ([System.IO.Path]::GetFileNameWithoutExtension($confFileName) + "_conf")
W "      <Component Id=`"ConfComponent`" Guid=`"E7A3B1D4-5F28-4C9A-A6E1-8D0F2B7C3E95`">"
W "        <File Id=`"$confFileId`" Source=`"$confFileName`" KeyPath=`"yes`" />"
W '      </Component>'
W '    </DirectoryRef>'
W ''

# ================================================== #
# Start Menu shortcuts                               #
# ================================================== #
W '    <!-- Start Menu shortcuts -->'
W '    <DirectoryRef Id="ApplicationProgramsFolder">'

$isFirstStartMenu = $true
foreach ($cmd in $cmds) {
    $compId = "Shortcut_$($cmd.name)_StartMenu"
    $guid = Get-DeterministicGuid -Seed "startmenu_$($cmd.name)_v2"
    $scId = "StartMenu_$($cmd.name)"
    $desc = if ($cmd.label -ne "") { $cmd.label } else { $cmd.name }
    $args = if ($cmd.name -ne "ssh") { $cmd.name } else { "ssh" }
    $iconRef = if ($declaredIcons.ContainsKey($cmd.icon)) { $declaredIcons[$cmd.icon] } else { $null }

    W "      <Component Id=`"$compId`" Guid=`"$guid`">"
    $scLine = "        <Shortcut Id=`"$scId`""
    $scLine += "`n                  Name=`"$($cmd.name)`""
    $scLine += "`n                  Description=`"$desc`""
    $scLine += "`n                  Target=`"[INSTALLFOLDER]openwrt-connect.exe`""
    if ($args -ne "") {
        $scLine += "`n                  Arguments=`"$args`""
    }
    $scLine += "`n                  WorkingDirectory=`"INSTALLFOLDER`""
    if ($iconRef) {
        $scLine += "`n                  Icon=`"$iconRef`""
    }
    $scLine += "/>"
    W $scLine

    # RemoveFolder on first component only
    if ($isFirstStartMenu) {
        W '        <RemoveFolder Id="CleanUpShortCut" Directory="ApplicationProgramsFolder" On="uninstall"/>'
        $isFirstStartMenu = $false
    }

    $regName = "shortcut_$($cmd.name)_start"
    W "        <RegistryValue Root=`"HKCU`" Key=`"Software\$($gen.product_name)`" Name=`"$regName`" Type=`"integer`" Value=`"1`" KeyPath=`"yes`"/>"
    W '      </Component>'
    W ''
}

W '    </DirectoryRef>'
W ''

# ================================================== #
# Desktop shortcuts                                  #
# ================================================== #
W '    <!-- Desktop shortcuts -->'
W '    <DirectoryRef Id="DesktopFolder">'

foreach ($cmd in $cmds) {
    $compId = "Shortcut_$($cmd.name)_Desktop"
    $guid = Get-DeterministicGuid -Seed "desktop_$($cmd.name)_v2"
    $scId = "Desktop_$($cmd.name)"
    $desc = if ($cmd.label -ne "") { $cmd.label } else { $cmd.name }
    $args = if ($cmd.name -ne "ssh") { $cmd.name } else { "ssh" }
    $iconRef = if ($declaredIcons.ContainsKey($cmd.icon)) { $declaredIcons[$cmd.icon] } else { $null }

    W "      <Component Id=`"$compId`" Guid=`"$guid`">"
    $scLine = "        <Shortcut Id=`"$scId`""
    $scLine += "`n                  Name=`"$($cmd.name)`""
    $scLine += "`n                  Description=`"$desc`""
    $scLine += "`n                  Target=`"[INSTALLFOLDER]openwrt-connect.exe`""
    if ($args -ne "") {
        $scLine += "`n                  Arguments=`"$args`""
    }
    $scLine += "`n                  WorkingDirectory=`"INSTALLFOLDER`""
    if ($iconRef) {
        $scLine += "`n                  Icon=`"$iconRef`""
    }
    $scLine += "/>"
    W $scLine

    $regName = "shortcut_$($cmd.name)_desktop"
    W "        <RegistryValue Root=`"HKCU`" Key=`"Software\$($gen.product_name)`" Name=`"$regName`" Type=`"integer`" Value=`"1`" KeyPath=`"yes`"/>"
    W '      </Component>'
    W ''
}

W '    </DirectoryRef>'
W ''

# ================================================== #
# UI                                                 #
# ================================================== #
W "    <WixVariable Id=`"WixUILicenseRtf`" Value=`"$LicenseFile`" />"
W '    <UI>'
W '      <UIRef Id="WixUI_FeatureTree" />'
W '    </UI>'
W ''
W '  </Product>'
W '</Wix>'

# ================================================== #
# Write output                                       #
# ================================================== #
$xml.ToString() | Out-File -FilePath $OutputFile -Encoding UTF8
Write-Host "Generated: $OutputFile"
Write-Host "  Source: $iniFileName (build settings) + $confFileName (runtime config)"
Write-Host "  Commands: $($cmds.Count)"
foreach ($cmd in $cmds) {
    Write-Host "    $($cmd.name) - $($cmd.label)"
}
