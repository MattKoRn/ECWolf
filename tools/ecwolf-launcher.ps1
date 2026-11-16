# ECWolf Windows GUI launcher. Does not modify existing engine config or save files.
param([switch]$SmokeCheck)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root 'ecwolf.exe'
$validator = Join-Path $PSScriptRoot 'game-data-validation.ps1'
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "Missing ECWolf engine: $exe" }
if (-not (Test-Path -LiteralPath $validator -PathType Leaf)) { throw "Missing data validator: $validator" }
. $validator
if ($SmokeCheck) { Write-Host 'ECWolf launcher file-layout check passed'; exit 0 }
$settingsFolder = Join-Path $env:LOCALAPPDATA 'ECWolf\Launcher'
$settingsFile = Join-Path $settingsFolder 'settings.json'
$settings = @{ Data = ''; Mods = ''; Display = '1920x1080'; Fullscreen = $true; Autoplayer = $false; SelectedMods = @() }
if (Test-Path -LiteralPath $settingsFile) {
    try {
        $prior = Get-Content -LiteralPath $settingsFile -Raw | ConvertFrom-Json
        foreach ($key in @('Data','Mods','Display','Fullscreen','Autoplayer','SelectedMods')) {
            if ($null -ne $prior.PSObject.Properties[$key]) { $settings[$key] = $prior.$key }
        }
    } catch { Write-Warning 'Launcher preferences are unreadable; defaults restored.' }
}
function Get-Editions([string]$directory) {
    $files = @(Get-ECWolfGameDataFiles -Directory $directory)
    $names = @{}
    foreach ($file in $files) { $names[$file.Name.ToUpperInvariant()] = $true }
    foreach ($ext in @('WL6','WL1','SOD')) {
        if ($names.ContainsKey("GAMEMAPS.$ext") -and $names.ContainsKey("MAPHEAD.$ext") -and $names.ContainsKey("VSWAP.$ext")) { $ext }
    }
}
function Quote-Arg([string]$s) {
    if ($s -match '\s') { return '"' + $s + '"' }
    return $s
}
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[System.Windows.Forms.Application]::EnableVisualStyles()
$bg = [System.Drawing.Color]::FromArgb(19,25,36)
$panel = [System.Drawing.Color]::FromArgb(35,46,62)
$accent = [System.Drawing.Color]::FromArgb(31,166,177)
$form = [System.Windows.Forms.Form]::new()
$form.Text = 'ECWolf Launcher'
$form.ClientSize = [System.Drawing.Size]::new(860,615)
$form.FormBorderStyle = 'FixedDialog'
$form.MaximizeBox = $false
$form.StartPosition = 'CenterScreen'
$form.BackColor = $bg
$form.ForeColor = [System.Drawing.Color]::White
$form.Font = [System.Drawing.Font]::new('Segoe UI',10)
function Label([string]$s,[int]$x,[int]$y,[int]$w,[int]$h,[int]$size=10) {
    $v = [System.Windows.Forms.Label]::new()
    $v.Text=$s; $v.Location=[System.Drawing.Point]::new($x,$y); $v.Size=[System.Drawing.Size]::new($w,$h)
    $v.Font=[System.Drawing.Font]::new('Segoe UI',$size)
    $form.Controls.Add($v); return $v
}
function TextBox([int]$x,[int]$y,[int]$w,[string]$value) {
    $v=[System.Windows.Forms.TextBox]::new()
    $v.Location=[System.Drawing.Point]::new($x,$y); $v.Size=[System.Drawing.Size]::new($w,28)
    $v.Text=$value; $v.BackColor=$panel; $v.ForeColor=[System.Drawing.Color]::White
    $form.Controls.Add($v); return $v
}
function Button([string]$s,[int]$x,[int]$y,[int]$w,[int]$h) {
    $v=[System.Windows.Forms.Button]::new()
    $v.Text=$s; $v.Location=[System.Drawing.Point]::new($x,$y); $v.Size=[System.Drawing.Size]::new($w,$h)
    $v.FlatStyle='Flat'; $v.BackColor=$panel; $v.ForeColor=[System.Drawing.Color]::White
    $form.Controls.Add($v); return $v
}
function Browse([System.Windows.Forms.TextBox]$target) {
    $dialog=[System.Windows.Forms.FolderBrowserDialog]::new()
    $dialog.Description='Select a local folder containing your own files.'
    if (Test-Path -LiteralPath $target.Text -PathType Container) { $dialog.SelectedPath=$target.Text }
    if ($dialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) { $target.Text=$dialog.SelectedPath }
    $dialog.Dispose()
}
[void](Label 'ECWOLF  /  WINDOWS EDITION' 24 17 800 45 21)
[void](Label 'Local game data  |  Select mods  |  Widescreen visual presets  |  RPG and Autoplayer' 26 64 800 24)
[void](Label 'GAME DATA DIRECTORY (FILES FROM A GAME YOU LEGALLY OWN)' 26 103 790 25)
$data=TextBox 26 131 701 ([string]$settings.Data)
$dataBrowse=Button 'Browse' 739 128 95 32
[void](Label 'GAME EDITION' 26 177 270 25)
$editions=[System.Windows.Forms.ComboBox]::new()
$editions.DropDownStyle='DropDownList'; $editions.Location=[System.Drawing.Point]::new(26,205)
$editions.Size=[System.Drawing.Size]::new(260,30); $editions.BackColor=$panel
$editions.ForeColor=[System.Drawing.Color]::White; $form.Controls.Add($editions)
[void](Label 'OPTIONAL MOD DIRECTORY (PK3, PK7, WAD, ZIP)' 26 247 790 25)
$mods=TextBox 26 275 701 ([string]$settings.Mods)
$modsBrowse=Button 'Browse' 739 272 95 32
$modList=[System.Windows.Forms.CheckedListBox]::new()
$modList.Location=[System.Drawing.Point]::new(26,316)
$modList.Size=[System.Drawing.Size]::new(808,125); $modList.CheckOnClick=$true
$modList.BackColor=$panel; $modList.ForeColor=[System.Drawing.Color]::White
$modList.HorizontalScrollbar=$true; $form.Controls.Add($modList)
[void](Label 'DISPLAY' 26 459 270 25)
$display=[System.Windows.Forms.ComboBox]::new()
$display.DropDownStyle='DropDownList'; $display.Location=[System.Drawing.Point]::new(26,488)
$display.Size=[System.Drawing.Size]::new(270,30)
[void]$display.Items.AddRange([object[]]@('1920x1080','1280x720','640x480'))
$display.SelectedItem=[string]$settings.Display
if ($display.SelectedIndex -lt 0) { $display.SelectedIndex=0 }
$display.BackColor=$panel; $display.ForeColor=[System.Drawing.Color]::White
$form.Controls.Add($display)
$fullscreen=[System.Windows.Forms.CheckBox]::new()
$fullscreen.Text='Fullscreen'; $fullscreen.Location=[System.Drawing.Point]::new(326,487)
$fullscreen.Size=[System.Drawing.Size]::new(145,32); $fullscreen.Checked=[bool]$settings.Fullscreen
$form.Controls.Add($fullscreen)
$autobot=[System.Windows.Forms.CheckBox]::new()
$autobot.Text='Start Autoplayer'; $autobot.Location=[System.Drawing.Point]::new(490,487)
$autobot.Size=[System.Drawing.Size]::new(180,32); $autobot.Checked=[bool]$settings.Autoplayer
$form.Controls.Add($autobot)
$status=Label 'Choose your game data directory to begin.' 26 556 565 43
$play=Button 'PLAY ECWOLF' 643 544 191 49
$play.BackColor=$accent
function Update-Editions {
    $editions.Items.Clear()
    try {
        foreach ($edition in @(Get-Editions $data.Text)) { [void]$editions.Items.Add($edition) }
        if ($editions.Items.Count -gt 0) { $editions.SelectedIndex=0; $status.Text='Ready to play.' }
        else { $status.Text='No complete supported game data set found.' }
    } catch { $status.Text='Select a folder with complete WL6, WL1 or SOD game data.' }
    $play.Enabled=($editions.Items.Count -gt 0)
}
function Update-Mods {
    $modList.Items.Clear()
    if (-not (Test-Path -LiteralPath $mods.Text -PathType Container)) { return }
    $files=@(Get-ChildItem -LiteralPath $mods.Text -File |
        Where-Object { @('.pk3','.pk7','.wad','.zip') -contains $_.Extension.ToLowerInvariant() } |
        Sort-Object Name)
    foreach ($file in $files) { [void]$modList.Items.Add($file.FullName, (@($settings.SelectedMods) -contains $file.FullName)) }
}
$dataBrowse.Add_Click({ Browse $data })
$modsBrowse.Add_Click({ Browse $mods })
$data.Add_TextChanged({ Update-Editions })
$mods.Add_TextChanged({ Update-Mods })
$play.Add_Click({
    try {
        $edition=[string]$editions.SelectedItem
        if (@(Get-Editions $data.Text) -notcontains $edition) { throw 'Game data is incomplete or missing.' }
        $resolution=([string]$display.SelectedItem).Split('x')
        $aspect=if ($display.SelectedItem -eq '640x480') { '4:3' } else { '16:9' }
        $arguments=@('--data',$edition,'--res',$resolution[0],$resolution[1],'--aspect',$aspect)
        if ($fullscreen.Checked) { $arguments+='--fullscreen' } else { $arguments+='--windowed' }
        if ($autobot.Checked) { $arguments+='--autoplay' }
        $selected=@()
        foreach ($mod in $modList.CheckedItems) {
            if (-not (Test-Path -LiteralPath ([string]$mod) -PathType Leaf)) { throw "Missing mod: $mod" }
            $selected += [string]$mod
            $arguments += @('--file',[string]$mod)
        }
        $settings.Data=$data.Text; $settings.Mods=$mods.Text; $settings.Display=[string]$display.SelectedItem
        $settings.Fullscreen=$fullscreen.Checked; $settings.Autoplayer=$autobot.Checked; $settings.SelectedMods=$selected
        New-Item -ItemType Directory -Path $settingsFolder -Force | Out-Null
        $settings | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $settingsFile -Encoding UTF8
        $line=($arguments | ForEach-Object { Quote-Arg ([string]$_) }) -join ' '
        Start-Process -FilePath $exe -WorkingDirectory $data.Text -ArgumentList $line
        $status.Text='Game started. Existing saves and RPG profile are untouched.'
    } catch {
        [void][System.Windows.Forms.MessageBox]::Show($_.Exception.Message,'ECWolf launcher error','OK','Error')
    }
})
Update-Editions
Update-Mods
[void]$form.ShowDialog()
$form.Dispose()
