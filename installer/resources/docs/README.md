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

- `index.html` - The main HTML file (generated output)
- `content/` - Individual content section files
- `templates/` - Template files for the documentation structure
  - `base.html` - Base HTML template with overall structure
  - `sidebar.html` - Navigation sidebar template
  - `content-placeholders.html` - Content placeholders template
- `js/`, `css/`, `*-images/` - Supporting files

## Customizing the Documentation

### Editing Content

Edit the HTML files in the `content/` directory to update the documentation content.

### Modifying the Sidebar Navigation

To change the sidebar navigation structure, edit the `templates/sidebar.html` file.

### Changing the Overall Layout

To modify the overall layout, edit the `templates/base.html` file.

## Rebuilding After Changes

After making any changes to content or templates, rebuild the documentation by running:

```
node build-docs.js
```