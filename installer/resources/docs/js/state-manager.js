/**
 * State Manager for Mach1 Documentation
 * Handles saving and restoring state (current section, scroll position, etc.)
 */

function saveCurrentState(contentId, targetId) {
    const state = {
        contentId: contentId,
        targetId: targetId,
        scrollPosition: window.pageYOffset
    };
    localStorage.setItem('mach1DocState', JSON.stringify(state));
}

function restoreState() {
    if (window.location.hash) {
        return {
            type: 'hash',
            hash: window.location.hash
        };
    }
    
    return {
        type: 'default'
    };
}

function setupBeforeUnloadHandler() {
    window.addEventListener('beforeunload', function() {
        const activeContent = document.querySelector('.product-content.active');
        if (activeContent && activeContent.id) {
            const contentId = activeContent.id.replace('-content', '');
            const activeLink = document.querySelector('.toc-sidebar a.active');
            const targetId = activeLink ? activeLink.getAttribute('href') : null;
            saveCurrentState(contentId, targetId);
        }
    });
} 