const fs = require('fs');
const path = require('path');

// Use __dirname to get the absolute path of the directory where the script is located
const docsDir = __dirname;
const contentDir = path.join(docsDir, 'content');

console.log('Reading index.html template...');
const indexHTML = fs.readFileSync(path.join(docsDir, 'index.html'), 'utf8');

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

console.log('Generating static HTML...');
let outputHTML = indexHTML;

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
console.log(`2. Run this script again`); 