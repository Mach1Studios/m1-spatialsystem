const fs = require('fs');
const path = require('path');

// Use __dirname to get the absolute path of the directory where the script is located
const docsDir = __dirname;
const contentDir = path.join(docsDir, 'content');
const templatesDir = path.join(docsDir, 'templates');

// Ensure templates directory exists
if (!fs.existsSync(templatesDir)) {
    fs.mkdirSync(templatesDir, { recursive: true });
    console.log('Created templates directory');
}

// Read templates
console.log('Reading templates...');
let baseTemplate, sidebarTemplate, contentPlaceholdersTemplate;

try {
    baseTemplate = fs.readFileSync(path.join(templatesDir, 'base.html'), 'utf8');
} catch (error) {
    console.log('Base template not found, creating it...');
    // Create base template file if it doesn't exist
    baseTemplate = `<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Mach1 Spatial System Documentation</title>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/5.15.4/css/all.min.css">
    <link rel="stylesheet" href="css/styles.css">
</head>
<body>
    <div class="container">
        <div id="sidebar" class="toc-sidebar active">
            <div class="toc-sidebar-inner">
                <h3>Documentation</h3>
                
                <!-- Search container -->
                <div id="search-container">
                    <div class="search-input-wrapper">
                        <input type="text" id="search-input" placeholder="Search documentation...">
                        <button id="clear-search" aria-label="Clear search">
                            <i class="fas fa-times"></i>
                        </button>
                    </div>
                    <div id="search-results"></div>
                </div>
                
                <!-- SIDEBAR_NAVIGATION -->
            </div>
        </div>
        
        <!-- Main content area -->
        <div class="main-content">
            <!-- Documentation content area -->
            <div class="doc-content">
                <!-- CONTENT_PLACEHOLDERS -->
            </div>
        </div>
    </div>

    <!-- Print button -->
    <div id="print-button" title="Print or save as PDF">
        <i class="fas fa-print"></i>
    </div>

    <script src="js/state-manager.js"></script>
    <script src="js/search.js"></script>
    <script src="js/print.js"></script>
    <script src="js/main.js"></script>
</body>
</html>`;
    fs.writeFileSync(path.join(templatesDir, 'base.html'), baseTemplate);
}

try {
    sidebarTemplate = fs.readFileSync(path.join(templatesDir, 'sidebar.html'), 'utf8');
} catch (error) {
    console.log('Sidebar template not found, creating it...');
    // Create sidebar template file if it doesn't exist
    sidebarTemplate = `<ul>
    <li class="has-submenu">
        <a href="#panner">Mach1 Panner</a>
        <ul>
            <li><a href="#panner-interface">Interface</a></li>
            <li><a href="#panner-description">Description</a></li>
            <li><a href="#panner-parameters">Plugin Parameters</a></li>
            <li><a href="#panner-general-params">General Parameters</a></li>
            <li><a href="#panner-stereo-params">Stereo Parameters</a></li>
            <li><a href="#panner-notes">Notes</a></li>
        </ul>
    </li>
    <li class="has-submenu">
        <a href="#monitor">Mach1 Monitor</a>
        <ul>
            <li><a href="#monitor-interface">Interface</a></li>
            <li><a href="#description">Description</a></li>
            <li><a href="#decoding">Decoding Explained</a></li>
            <li><a href="#headlocked">Headlocked/Static Stereo</a></li>
            <li><a href="#plugin-features">Plugin Features</a></li>
            <li><a href="#general-parameters">General Parameters</a></li>
            <li><a href="#settings">Settings</a></li>
            <li><a href="#monitor-modes">Monitor Modes</a></li>
            <li><a href="#timecode-offset">Timecode Offset</a></li>
            <li><a href="#upcoming-features">Upcoming Features</a></li>
        </ul>
    </li>
    <li class="has-submenu">
        <a href="#player">Mach1 Player</a>
        <ul>
            <li><a href="#player-interface">Interface</a></li>
            <li><a href="#player-overview">Overview</a></li>
            <li><a href="#player-features">Features</a></li>
            <li><a href="#player-playback">Playback</a></li>
            <li><a href="#player-getting-started">Getting Started</a></li>
        </ul>
    </li>
    <li class="has-submenu">
        <a href="#transcoder">Mach1 Transcoder</a>
        <ul>
            <li><a href="#transcoder-interface">Interface</a></li>
            <li><a href="#transcoder-description">Description</a></li>
            <li><a href="#transcoder-spatial-format">Mach1 Spatial Format</a></li>
            <li><a href="#transcoder-explained">Transcoder Explained</a></li>
            <li><a href="#transcoder-conversion">Format Conversion</a></li>
            <li><a href="#transcoder-headlocked">Headlocked/Static Stereo</a></li>
            <li><a href="#transcoder-features">Transcoding Features</a></li>
            <li><a href="#transcoder-input">Input Options</a></li>
            <li><a href="#transcoder-output">Output Types</a></li>
            <li><a href="#transcoder-additional">Additional Features</a></li>
        </ul>
    </li>
    <li class="has-submenu">
        <a href="#guide">User Guide</a>
        <ul>
            <li><a href="#guide-multichannel">Mach1 Spatial Multichannel Layouts</a></li>
            <li><a href="#guide-format-description">Format Description</a></li>
            <li><a href="#guide-spatial">Mach1 Spatial</a></li>
            <li><a href="#guide-deliverable">Multichannel Deliverable</a></li>
            <li><a href="#guide-tools">Mach1 Spatial System Tools</a></li>
            <li><a href="#guide-general-params">General Parameters</a></li>
            <li><a href="#guide-deployment">Game Engine Deployment</a></li>
            <li><a href="#guide-sdk">M1 Spatial SDK Integration</a></li>
            <li><a href="#guide-hacker">System Hacker Help</a></li>
        </ul>
    </li>
</ul>`;
    fs.writeFileSync(path.join(templatesDir, 'sidebar.html'), sidebarTemplate);
}

try {
    contentPlaceholdersTemplate = fs.readFileSync(path.join(templatesDir, 'content-placeholders.html'), 'utf8');
} catch (error) {
    console.log('Content placeholders template not found, creating it...');
    // Create content placeholders template file if it doesn't exist
    contentPlaceholdersTemplate = `<div id="panner-content" class="product-content"></div>
<div id="monitor-content" class="product-content"></div>
<div id="player-content" class="product-content"></div>
<div id="transcoder-content" class="product-content"></div>
<div id="guide-content" class="product-content"></div>`;
    fs.writeFileSync(path.join(templatesDir, 'content-placeholders.html'), contentPlaceholdersTemplate);
}

// Read content files
function readContentFile(filename) {
    const filePath = path.join(contentDir, filename);
    console.log(`Reading ${filePath}...`);
    try {
        return fs.readFileSync(filePath, 'utf8');
    } catch (error) {
        console.error(`Error reading ${filePath}: ${error.message}`);
        return `<h2>Error: Could not load ${filename}</h2>`;
    }
}

const pannerContent = readContentFile('panner.html');
const monitorContent = readContentFile('monitor.html');
const playerContent = readContentFile('player.html');
const transcoderContent = readContentFile('transcoder.html');
const guideContent = readContentFile('guide.html');

// Build the final HTML
console.log('Generating static HTML...');

// Replace placeholders in the base template
let outputHTML = baseTemplate
    .replace('<!-- SIDEBAR_NAVIGATION -->', sidebarTemplate)
    .replace('<!-- CONTENT_PLACEHOLDERS -->', contentPlaceholdersTemplate);

// Replace content in the placeholders
function replaceContentInDiv(html, divId, newContent) {
    const regex = new RegExp(`(<div id="${divId}" class="product-content">).*?(</div>)`, 's');
    return html.replace(regex, `$1${newContent}$2`);
}

outputHTML = replaceContentInDiv(outputHTML, 'panner-content', pannerContent);
outputHTML = replaceContentInDiv(outputHTML, 'monitor-content', monitorContent);
outputHTML = replaceContentInDiv(outputHTML, 'player-content', playerContent);
outputHTML = replaceContentInDiv(outputHTML, 'transcoder-content', transcoderContent);
outputHTML = replaceContentInDiv(outputHTML, 'guide-content', guideContent);

// Ensure the js directory exists
const jsDir = path.join(docsDir, 'js');
if (!fs.existsSync(jsDir)) {
    fs.mkdirSync(jsDir, { recursive: true });
}

// Create search.js file if it doesn't exist
const searchJsPath = path.join(jsDir, 'search.js');
if (!fs.existsSync(searchJsPath)) {
    console.log('Creating search.js...');
    fs.writeFileSync(searchJsPath, `/**
 * Search functionality for Mach1 Documentation
 */

// Search implementation will be here
// This file is a placeholder that will be replaced with the actual implementation
`);
}

// Create print.js file if it doesn't exist
const printJsPath = path.join(jsDir, 'print.js');
if (!fs.existsSync(printJsPath)) {
    console.log('Creating print.js...');
    fs.writeFileSync(printJsPath, `/**
 * Print/PDF Export functionality for Mach1 Documentation
 */

// Print implementation will be here
// This file is a placeholder that will be replaced with the actual implementation
`);
}

const outputPath = path.join(docsDir, 'index.html');
fs.writeFileSync(outputPath, outputHTML);
console.log(`Static HTML file generated successfully: ${outputPath}`);
console.log(`\nTo rebuild the documentation after changes:`);
console.log(`1. Edit content files in the content/ directory`);
console.log(`2. Edit sidebar navigation in templates/sidebar.html`);
console.log(`3. Run this script again`); 