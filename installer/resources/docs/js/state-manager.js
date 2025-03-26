/**
 * State Manager for Mach1 Documentation
 * Handles saving and restoring state (current section, scroll position, etc.)
 */

// Function to save current state to localStorage
function saveCurrentState(contentId, targetId) {
    const state = {
        contentId: contentId,
        targetId: targetId,
        scrollPosition: window.pageYOffset
    };
    localStorage.setItem('mach1DocState', JSON.stringify(state));
}

// Function to restore state from localStorage or URL hash
function restoreState() {
    // Check if there's a hash in the URL (always takes priority)
    if (window.location.hash) {
        return {
            type: 'hash',
            hash: window.location.hash
        };
    }
    
    // Always return default to show Panner page
    return {
        type: 'default'
    };
    
    // This code is intentionally commented out to force default to Panner page
    /*
    // Otherwise check if there's a saved state
    const savedState = localStorage.getItem('mach1DocState');
    if (savedState) {
        try {
            // Try to parse the saved state
            const state = JSON.parse(savedState);
            if (state && state.contentId) {
                return {
                    type: 'saved',
                    state: state
                };
            }
        } catch (e) {
            console.error('Error parsing saved state:', e);
        }
    }
    
    // If no hash or valid saved state, return default
    return {
        type: 'default'
    };
    */
}

// Save state before unloading the page
function setupBeforeUnloadHandler() {
    window.addEventListener('beforeunload', function() {
        // Get current content section
        const activeContent = document.querySelector('.product-content.active');
        if (activeContent && activeContent.id) {
            const contentId = activeContent.id.replace('-content', '');
            // Get current active section
            const activeLink = document.querySelector('.toc-sidebar a.active');
            const targetId = activeLink ? activeLink.getAttribute('href') : null;
            // Save current state
            saveCurrentState(contentId, targetId);
        }
    });
} 