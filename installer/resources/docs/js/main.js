/**
 * Main JavaScript for Mach1 Documentation
 * Handles navigation, TOC accordion, content switching, etc.
 */

document.addEventListener('DOMContentLoaded', function() {
    // Setup dynamic content loading
    loadAllContentSections();
    
    // Setup TOC accordion
    setupTOCAccordion();
    
    // Get all navigation links
    const navLinks = document.querySelectorAll('.product-list a');
    
    // Get all content sections
    const contentSections = document.querySelectorAll('.product-content');
    
    // Get all TOC links
    const tocLinks = document.querySelectorAll('.toc-sidebar a');
    
    // Add click event listeners for product navigation
    setupNavigationLinks(navLinks, contentSections);
    
    // Add click handlers for TOC links
    setupTOCLinks(tocLinks, contentSections, navLinks);

    // Setup double-click handlers for top-level TOC headings
    setupTopLevelTOCDoubleclicks();

    // Add smooth scrolling for tooltip areas
    setupTooltipAreas();
    
    // Initialize page based on saved state or URL hash
    initializePage();
    
    // Setup scroll event for highlighting current section
    setupScrollHighlighting();
    
    // Setup beforeunload handler to save state
    setupBeforeUnloadHandler();
    
    // Show Panner content by default if no content is active yet
    if (!document.querySelector('.product-content.active')) {
        showDefaultPage();
    }
});

// Function to load all content sections
function loadAllContentSections() {
    const sections = ['panner', 'monitor', 'player', 'transcoder', 'guide'];
    
    // First load the panner content (high priority)
    fetch(`content/panner.html`)
        .then(response => response.text())
        .then(html => {
            document.getElementById('panner-content').innerHTML = html;
            
            // If no active content yet, make panner active
            if (!document.querySelector('.product-content.active')) {
                showDefaultPage();
            }
        })
        .catch(error => {
            console.error(`Error loading panner content:`, error);
        });
    
    // Then load other sections
    sections.forEach(section => {
        if (section === 'panner') return; // Already loaded
        
        fetch(`content/${section}.html`)
            .then(response => response.text())
            .then(html => {
                document.getElementById(`${section}-content`).innerHTML = html;
            })
            .catch(error => {
                console.error(`Error loading ${section} content:`, error);
            });
    });
}

// Setup TOC accordion
function setupTOCAccordion() {
    const sidebar = document.querySelector('#sidebar');
    const menuItems = sidebar.querySelectorAll('li');
    
    menuItems.forEach(item => {
        const subMenu = item.querySelector(':scope > ul');
        if (subMenu) {
            item.classList.add('has-submenu');
            item.classList.add('collapsed'); // Start collapsed
        }
    });
}

// Setup navigation links
function setupNavigationLinks(navLinks, contentSections) {
    navLinks.forEach(link => {
        link.addEventListener('click', function(e) {
            e.preventDefault();
            
            // Remove active class from all links
            navLinks.forEach(l => l.classList.remove('active'));
            
            // Add active class to clicked link
            this.classList.add('active');
            
            // Get the content ID from the href attribute
            const contentId = this.getAttribute('href').substring(1);
            
            // Save state
            saveCurrentState(contentId, null);
            
            // Hide all content sections
            contentSections.forEach(section => {
                section.classList.remove('active');
            });
            
            // Show the selected content section
            document.getElementById(contentId + '-content').classList.add('active');
            
            // Find and expand the corresponding TOC section
            const sectionLink = document.querySelector(`#sidebar .has-submenu > a[href="#${contentId}"]`);
            if (sectionLink) {
                // Collapse all TOC sections first
                document.querySelectorAll('#sidebar .has-submenu').forEach(item => {
                    item.classList.add('collapsed');
                });
                
                // Expand the selected section
                const parentLi = sectionLink.closest('.has-submenu');
                if (parentLi) {
                    parentLi.classList.remove('collapsed');
                }
            }
            
            // Reset scroll position for the selected content
            window.scrollTo({
                top: 0,
                behavior: 'smooth'
            });
        });
    });
}

// Setup TOC links
function setupTOCLinks(tocLinks, contentSections, navLinks) {
    // Variable to track what link was last clicked
    let userClickedLink = null;
    let clickTimestamp = 0;
    
    tocLinks.forEach(link => {
        link.addEventListener('click', function(e) {
            e.preventDefault();
            
            const targetId = this.getAttribute('href');
            const targetElement = document.querySelector(targetId);
            
            // Check if this is a parent menu item with submenu
            const parentLi = link.parentElement;
            const isSubmenuParent = parentLi.classList.contains('has-submenu');
            
            if (isSubmenuParent) {
                // Check if this is a top-level menu item (direct child of the main ul)
                const isTopLevel = parentLi.parentNode.parentNode.classList.contains('toc-sidebar-inner');
                
                // If it's a top-level accordion header
                if (isTopLevel) {
                    // Check if already expanded (not collapsed)
                    const isExpanded = !parentLi.classList.contains('collapsed');
                    
                    if (isExpanded) {
                        // If already expanded, navigate to the page instead of toggling
                        navigateToPage(this.getAttribute('href').substring(1));
                        return;
                    }
                    
                    // First collapse all top-level accordions
                    document.querySelectorAll('.toc-sidebar > .toc-sidebar-inner > ul > li.has-submenu').forEach(item => {
                        item.classList.add('collapsed');
                    });
                    
                    // Then expand only the clicked one
                    parentLi.classList.toggle('collapsed');
                } else {
                    // For non top-level items with submenu, just toggle
                    parentLi.classList.toggle('collapsed');
                }
                
                // If it has a submenu, don't navigate 
                if (parentLi.querySelector('ul')) {
                    return;
                }
            }
            
            // Only navigate if there's a target element
            if (targetElement) {
                // Record user click for smooth scrolling
                userClickedLink = this;
                clickTimestamp = Date.now();
                
                // Update active states
                tocLinks.forEach(l => l.classList.remove('active'));
                this.classList.add('active');
                
                // Expand parent sections
                let parent = this.closest('li.has-submenu');
                while (parent) {
                    parent.classList.remove('collapsed');
                    parent = parent.parentElement.closest('li.has-submenu');
                }
                
                // Find which product this belongs to and switch if needed
                let topParent = this.closest('.toc-sidebar > .toc-sidebar-inner > ul > li');
                let needsContentSwitch = false;
                let contentId = null;
                
                if (topParent) {
                    const topLink = topParent.querySelector(':scope > a');
                    if (topLink) {
                        const href = topLink.getAttribute('href');
                        if (href && href.startsWith('#')) {
                            contentId = href.substring(1);
                            const targetContent = document.getElementById(contentId + '-content');
                            
                            // Check if we need to switch content sections
                            const currentActive = document.querySelector('.product-content.active');
                            if (currentActive !== targetContent) {
                                needsContentSwitch = true;
                                
                                // Show the corresponding product content
                                contentSections.forEach(section => {
                                    section.classList.remove('active');
                                });
                                
                                if (targetContent) {
                                    targetContent.classList.add('active');
                                }
                                
                                // Update main navigation
                                navLinks.forEach(navLink => {
                                    navLink.classList.remove('active');
                                    if (navLink.getAttribute('href') === href) {
                                        navLink.classList.add('active');
                                    }
                                });
                            }
                        }
                    }
                }
                
                // Save the current state to localStorage
                saveCurrentState(contentId, targetId);
                
                // If we switched content sections, we need a slight delay before scrolling
                if (needsContentSwitch) {
                    setTimeout(() => {
                        // Calculate offset with minimal padding
                        const headerHeight = 60; // Estimated header height
                        const offset = targetElement.offsetTop - headerHeight;
                        
                        // Smooth scroll to target
                        window.scrollTo({
                            top: offset,
                            behavior: 'smooth'
                        });
                        
                        // Update URL without jumping
                        history.pushState(null, null, targetId);
                    }, 10);
                } else {
                    // If we didn't switch content, just scroll immediately
                    // Calculate offset with minimal padding
                    const headerHeight = 60; // Estimated header height
                    const offset = targetElement.offsetTop - headerHeight;
                    
                    // Smooth scroll to target
                    window.scrollTo({
                        top: offset,
                        behavior: 'smooth'
                    });
                    
                    // Update URL without jumping
                    history.pushState(null, null, targetId);
                }
            }
        });
    });
    
    // Track when user clicks a link for smoother scrolling experience
    tocLinks.forEach(link => {
        link.addEventListener('click', function() {
            userClickedLink = this;
            clickTimestamp = Date.now();
        });
    });
}

// Setup tooltip areas
function setupTooltipAreas() {
    const tooltipAreas = document.querySelectorAll('.tooltip-area[href]');
    tooltipAreas.forEach(area => {
        area.addEventListener('click', function(e) {
            e.preventDefault();
            const targetId = this.getAttribute('href');
            const targetElement = document.querySelector(targetId);
            
            if (targetElement) {
                // Find parent section ID for saving state
                let contentId = null;
                const parentSection = targetElement.closest('.product-content');
                if (parentSection && parentSection.id) {
                    contentId = parentSection.id.replace('-content', '');
                }
                
                // Save state
                saveCurrentState(contentId, targetId);
                
                // Calculate offset with minimal padding
                const headerHeight = 60; // Estimated header height
                const offset = targetElement.offsetTop - headerHeight;
                
                // Smooth scroll to target
                window.scrollTo({
                    top: offset,
                    behavior: 'smooth'
                });
                
                // Update URL without jumping
                history.pushState(null, null, targetId);
                
                // Find and activate the corresponding TOC link
                const tocLink = document.querySelector(`.toc-sidebar a[href="${targetId}"]`);
                if (tocLink) {
                    const tocLinks = document.querySelectorAll('.toc-sidebar a');
                    tocLinks.forEach(l => l.classList.remove('active'));
                    tocLink.classList.add('active');
                    
                    // Expand parent if in collapsed menu
                    let parent = tocLink.closest('li.has-submenu');
                    while (parent) {
                        parent.classList.remove('collapsed');
                        parent = parent.parentElement.closest('li.has-submenu');
                    }
                }
            }
        });
    });
}

// Initialize page based on saved state or URL hash
function initializePage() {
    const state = restoreState();
    
    switch (state.type) {
        case 'hash':
            handleHashNavigation(state.hash);
            break;
        case 'saved':
            restoreSavedState(state.state);
            break;
        case 'default':
        default:
            showDefaultPage();
            break;
    }
}

// Handle hash navigation (e.g., #panner-features)
function handleHashNavigation(hash) {
    const targetId = hash;
    const targetElement = document.querySelector(targetId);
    
    if (targetElement) {
        setTimeout(() => {
            // Find parent content
            let contentId = null;
            const parentSection = targetElement.closest('.product-content');
            if (parentSection && parentSection.id) {
                contentId = parentSection.id.replace('-content', '');
                
                // Show the corresponding product content
                const contentSections = document.querySelectorAll('.product-content');
                contentSections.forEach(section => {
                    section.classList.remove('active');
                });
                
                if (parentSection) {
                    parentSection.classList.add('active');
                }
                
                // Update main navigation
                const navLinks = document.querySelectorAll('.product-list a');
                navLinks.forEach(navLink => {
                    navLink.classList.remove('active');
                    if (navLink.getAttribute('href') === '#' + contentId) {
                        navLink.classList.add('active');
                    }
                });
            }
            
            // Calculate offset with minimal padding
            const headerHeight = 60; // Estimated header height
            const offset = targetElement.offsetTop - headerHeight;
            
            window.scrollTo({
                top: offset,
                behavior: 'smooth'
            });
            
            // Find and activate the correct TOC link
            const tocLink = document.querySelector(`.toc-sidebar a[href="${targetId}"]`);
            if (tocLink) {
                const tocLinks = document.querySelectorAll('.toc-sidebar a');
                tocLinks.forEach(l => l.classList.remove('active'));
                tocLink.classList.add('active');
                
                // Expand parent accordions
                let parent = tocLink.closest('li.has-submenu');
                while (parent) {
                    parent.classList.remove('collapsed');
                    parent = parent.parentElement.closest('li.has-submenu');
                }
            }
        }, 300);
    }
}

// Restore state from localStorage
function restoreSavedState(savedState) {
    if (savedState.contentId) {
        // Show the corresponding content section
        const contentSections = document.querySelectorAll('.product-content');
        contentSections.forEach(section => {
            section.classList.remove('active');
        });
        
        const contentSection = document.getElementById(savedState.contentId + '-content');
        if (contentSection) {
            contentSection.classList.add('active');
            
            // Update main navigation
            const navLinks = document.querySelectorAll('.product-list a');
            navLinks.forEach(navLink => {
                navLink.classList.remove('active');
                if (navLink.getAttribute('href') === '#' + savedState.contentId) {
                    navLink.classList.add('active');
                }
            });
            
            // Expand the corresponding TOC section
            const sectionLink = document.querySelector(`#sidebar .has-submenu > a[href="#${savedState.contentId}"]`);
            if (sectionLink) {
                // Collapse all TOC sections first
                document.querySelectorAll('#sidebar .has-submenu').forEach(item => {
                    item.classList.add('collapsed');
                });
                
                // Expand the selected section
                const parentLi = sectionLink.closest('.has-submenu');
                if (parentLi) {
                    parentLi.classList.remove('collapsed');
                }
            }
            
            // If we have a specific target ID, scroll to it
            if (savedState.targetId) {
                const targetElement = document.querySelector(savedState.targetId);
                if (targetElement) {
                    setTimeout(() => {
                        const headerHeight = 60;
                        const offset = targetElement.offsetTop - headerHeight;
                        
                        window.scrollTo({
                            top: offset,
                            behavior: 'auto'
                        });
                        
                        // Find and activate the correct TOC link
                        const tocLinks = document.querySelectorAll('.toc-sidebar a');
                        const tocLink = document.querySelector(`.toc-sidebar a[href="${savedState.targetId}"]`);
                        if (tocLink) {
                            tocLinks.forEach(l => l.classList.remove('active'));
                            tocLink.classList.add('active');
                            
                            // Expand parent accordions
                            let parent = tocLink.closest('li.has-submenu');
                            while (parent) {
                                parent.classList.remove('collapsed');
                                parent = parent.parentElement.closest('li.has-submenu');
                            }
                        }
                    }, 100);
                } else {
                    // If no specific target element, restore scroll position
                    window.scrollTo(0, savedState.scrollPosition || 0);
                }
            } else {
                // If no specific target element, restore scroll position
                window.scrollTo(0, savedState.scrollPosition || 0);
            }
        }
    }
}

// Show default page (Mach1 Panner)
function showDefaultPage() {
    // Show Panner content
    const contentSections = document.querySelectorAll('.product-content');
    contentSections.forEach(section => {
        section.classList.remove('active');
    });
    
    const pannerContent = document.getElementById('panner-content');
    if (pannerContent) {
        pannerContent.classList.add('active');
    }
    
    // Activate panner link in main navigation
    const navLinks = document.querySelectorAll('.product-list a');
    navLinks.forEach(navLink => {
        navLink.classList.remove('active');
        if (navLink.getAttribute('href') === '#panner') {
            navLink.classList.add('active');
        }
    });
    
    // Expand the panner section in TOC
    const pannerLink = document.querySelector('#sidebar .has-submenu > a[href="#panner"]');
    if (pannerLink) {
        // Collapse all TOC sections first
        document.querySelectorAll('#sidebar .has-submenu').forEach(item => {
            item.classList.add('collapsed');
        });
        
        // Expand the panner section
        const parentLi = pannerLink.closest('.has-submenu');
        if (parentLi) {
            parentLi.classList.remove('collapsed');
        }
    }
}

// Setup scroll event for highlighting current section
function setupScrollHighlighting() {
    // Variable to track what link was last clicked
    let userClickedLink = null;
    let clickTimestamp = 0;
    
    // Helper function to determine if an element is in viewport
    function isElementInViewport(el) {
        const rect = el.getBoundingClientRect();
        const windowHeight = window.innerHeight || document.documentElement.clientHeight;
        const headerOffset = 100; // Account for header/navigation
        
        // Calculate how much of the element is visible
        const visibleHeight = Math.min(rect.bottom, windowHeight) - Math.max(rect.top, headerOffset);
        const elementVisibleRatio = visibleHeight / rect.height;
        
        // Check if element is entering from top of viewport
        const isEnteringTop = rect.top >= headerOffset && rect.top <= windowHeight * 0.5;
        
        // Check if element's top is close to the top of the viewport
        const isNearTop = rect.top >= -100 && rect.top <= windowHeight * 0.3;
        
        return isEnteringTop || elementVisibleRatio > 0.3 || isNearTop;
    }
    
    // Function to highlight current section based on scroll position
    function highlightCurrentSection() {
        // Skip if a user just clicked a link (within last 1 second)
        if (userClickedLink && (Date.now() - clickTimestamp) < 1000) {
            return;
        }
        
        // Get the active product content
        const activeProduct = document.querySelector('.product-content.active');
        if (!activeProduct) return;
        
        // Get all sections with IDs
        const sections = activeProduct.querySelectorAll('.doc-section[id], .doc-subsection[id], h4[id]');
        
        // Find visible sections
        let visibleSections = [];
        sections.forEach(section => {
            if (isElementInViewport(section)) {
                visibleSections.push({
                    id: section.id,
                    top: section.getBoundingClientRect().top
                });
            }
        });
        
        // Sort by position (top to bottom)
        visibleSections.sort((a, b) => a.top - b.top);
        
        // Find the topmost visible section
        if (visibleSections.length > 0) {
            // Prefer sections at the top of the viewport
            let currentSectionId = null;
            
            for (let i = 0; i < visibleSections.length; i++) {
                if (visibleSections[i].top >= -100) { // Just below the top of viewport
                    currentSectionId = visibleSections[i].id;
                    break;
                }
            }
            
            // If no preferred section found, use the first visible one
            if (!currentSectionId && visibleSections.length > 0) {
                currentSectionId = visibleSections[0].id;
            }
            
            if (currentSectionId) {
                // Find parent content for saving state
                let contentId = null;
                const activeContent = document.querySelector('.product-content.active');
                if (activeContent && activeContent.id) {
                    contentId = activeContent.id.replace('-content', '');
                }
                
                // Save current section to localStorage while scrolling
                saveCurrentState(contentId, '#' + currentSectionId);
                
                // Find the TOC link for the current section
                const tocLinks = document.querySelectorAll('.toc-sidebar a');
                const activeTocLink = document.querySelector(`#sidebar a[href="#${currentSectionId}"]`);
                if (activeTocLink) {
                    // Update active states
                    tocLinks.forEach(l => l.classList.remove('active'));
                    activeTocLink.classList.add('active');
                    
                    // Expand parent sections
                    let parent = activeTocLink.closest('li.has-submenu');
                    while (parent) {
                        parent.classList.remove('collapsed');
                        parent = parent.parentElement.closest('li.has-submenu');
                    }
                }
            }
        }
    }
    
    // Throttle scroll events for better performance
    let scrollTimeout;
    window.addEventListener('scroll', function() {
        clearTimeout(scrollTimeout);
        scrollTimeout = setTimeout(highlightCurrentSection, 100);
    });
    
    // Call highlighting on initial load
    setTimeout(highlightCurrentSection, 500);
}

// Setup double-click handlers for top-level TOC headings
function setupTopLevelTOCDoubleclicks() {
    const topLevelLinks = document.querySelectorAll('.toc-sidebar > .toc-sidebar-inner > ul > li.has-submenu > a');
    
    topLevelLinks.forEach(link => {
        link.addEventListener('dblclick', function(e) {
            e.preventDefault();
            
            // Get the content ID from the href attribute
            const contentId = this.getAttribute('href').substring(1);
            
            // First collapse all top-level accordions
            document.querySelectorAll('.toc-sidebar > .toc-sidebar-inner > ul > li.has-submenu').forEach(item => {
                item.classList.add('collapsed');
            });
            
            // Then expand only the clicked one
            const parentLi = this.closest('li.has-submenu');
            if (parentLi) {
                parentLi.classList.remove('collapsed');
            }
            
            // Use the shared navigation function
            navigateToPage(contentId);
        });
    });
}

// Function to navigate to a page (used by both click and double-click handlers)
function navigateToPage(contentId) {
    // Hide all content sections
    const contentSections = document.querySelectorAll('.product-content');
    contentSections.forEach(section => {
        section.classList.remove('active');
    });
    
    // Show the selected content section
    const targetContent = document.getElementById(contentId + '-content');
    if (targetContent) {
        targetContent.classList.add('active');
    }
    
    // Update active states for TOC links
    document.querySelectorAll('.toc-sidebar a').forEach(l => l.classList.remove('active'));
    const selectedLink = document.querySelector(`.toc-sidebar a[href="#${contentId}"]`);
    if (selectedLink) {
        selectedLink.classList.add('active');
    }
    
    // Update main navigation links
    const navLinks = document.querySelectorAll('.product-list a');
    navLinks.forEach(navLink => {
        navLink.classList.remove('active');
        if (navLink.getAttribute('href') === `#${contentId}`) {
            navLink.classList.add('active');
        }
    });
    
    // Save state
    saveCurrentState(contentId, null);
    
    // Reset scroll position with a slight delay to ensure content is loaded and rendering is complete
    setTimeout(() => {
        window.scrollTo({
            top: 0,
            behavior: 'auto' // Use 'auto' instead of 'smooth' for more reliable scrolling
        });
    }, 50);
    
    // Also immediately try to scroll - this often works for already-loaded content
    window.scrollTo({
        top: 0,
        behavior: 'auto'
    });
} 