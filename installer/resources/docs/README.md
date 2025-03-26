# Mach1 Documentation System

This folder contains the Mach1 Spatial System documentation.

## Prerequisites

- [Node.js](https://nodejs.org/) (v12 or higher)

## Building the Documentation

### Windows (PowerShell or Command Prompt)

```
cd installer\resources\docs
node build-docs.js
```

### macOS / Linux

```bash
cd installer/resources/docs
node build-docs.js
```

## Structure

- `index.html` - The main HTML file (both template and built output)
- `content/` - Individual content section files
- `js/`, `css/`, `*-images/` - Supporting files