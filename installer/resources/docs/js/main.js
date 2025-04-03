/**
 * Main JavaScript for Mach1 Documentation
 * Handles navigation, TOC accordion, content switching, etc.
 */

document.addEventListener('DOMContentLoaded', function() {
    setupTOCAccordion();
    
    const navLinks = document.querySelectorAll('.product-list a');
    
    const contentSections = document.querySelectorAll('.product-content');
    
    const tocLinks = document.querySelectorAll('.toc-sidebar a');
    
    setupNavigationLinks(navLinks, contentSections);
    
    setupTOCLinks(tocLinks, contentSections, navLinks);

    setupTopLevelTOCDoubleclicks();

    setupTooltipAreas();
    
    initializePage();
    
    setupScrollHighlighting();
    
    setupBeforeUnloadHandler();
    
    if (!document.querySelector('.product-content.active')) {
        showDefaultPage();
    }
});

function setupTOCAccordion() {
    const sidebar = document.querySelector('#sidebar');
    const menuItems = sidebar.querySelectorAll('li');
    
    menuItems.forEach(item => {
        const subMenu = item.querySelector(':scope > ul');
        if (subMenu) {
            item.classList.add('has-submenu');
            item.classList.add('collapsed');
        }
    });
}

function setupNavigationLinks(navLinks, contentSections) {
    navLinks.forEach(link => {
        link.addEventListener('click', function(e) {
            e.preventDefault();
            
            navLinks.forEach(l => l.classList.remove('active'));
            
            this.classList.add('active');
            
            const contentId = this.getAttribute('href').substring(1);
            
            saveCurrentState(contentId, null);
            
            contentSections.forEach(section => {
                section.classList.remove('active');
            });
            
            document.getElementById(contentId + '-content').classList.add('active');
            
            const sectionLink = document.querySelector(`#sidebar .has-submenu > a[href="#${contentId}"]`);
            if (sectionLink) {
                document.querySelectorAll('#sidebar .has-submenu').forEach(item => {
                    item.classList.add('collapsed');
                });
                
                const parentLi = sectionLink.closest('.has-submenu');
                if (parentLi) {
                    parentLi.classList.remove('collapsed');
                }
            }
            
            window.scrollTo({
                top: 0,
                behavior: 'smooth'
            });
        });
    });
}

function setupTOCLinks(tocLinks, contentSections, navLinks) {
    let userClickedLink = null;
    let clickTimestamp = 0;
    
    tocLinks.forEach(link => {
        link.addEventListener('click', function(e) {
            e.preventDefault();
            
            const targetId = this.getAttribute('href');
            const targetElement = document.querySelector(targetId);
            
            const parentLi = link.parentElement;
            const isSubmenuParent = parentLi.classList.contains('has-submenu');
            
            if (isSubmenuParent) {
                const isTopLevel = parentLi.parentNode.parentNode.classList.contains('toc-sidebar-inner');
                
                if (isTopLevel) {
                    const isExpanded = !parentLi.classList.contains('collapsed');
                    
                    if (isExpanded) {
                        navigateToPage(this.getAttribute('href').substring(1));
                        return;
                    }
                    
                    document.querySelectorAll('.toc-sidebar > .toc-sidebar-inner > ul > li.has-submenu').forEach(item => {
                        item.classList.add('collapsed');
                    });
                    
                    parentLi.classList.toggle('collapsed');
                } else {
                    parentLi.classList.toggle('collapsed');
                }
                
                if (parentLi.querySelector('ul')) {
                    return;
                }
            }
            
            if (targetElement) {
                userClickedLink = this;
                clickTimestamp = Date.now();
                
                tocLinks.forEach(l => l.classList.remove('active'));
                this.classList.add('active');
                
                let parent = this.closest('li.has-submenu');
                while (parent) {
                    parent.classList.remove('collapsed');
                    parent = parent.parentElement.closest('li.has-submenu');
                }
                
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
                            
                            const currentActive = document.querySelector('.product-content.active');
                            if (currentActive !== targetContent) {
                                needsContentSwitch = true;
                                
                                contentSections.forEach(section => {
                                    section.classList.remove('active');
                                });
                                
                                if (targetContent) {
                                    targetContent.classList.add('active');
                                }
                                
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
                
                saveCurrentState(contentId, targetId);
                
                if (needsContentSwitch) {
                    setTimeout(() => {
                        const headerHeight = 60;
                        const offset = targetElement.offsetTop - headerHeight;
                        
                        window.scrollTo({
                            top: offset,
                            behavior: 'smooth'
                        });
                        
                        history.pushState(null, null, targetId);
                    }, 10);
                } else {
                    const headerHeight = 60;
                    const offset = targetElement.offsetTop - headerHeight;
                    
                    window.scrollTo({
                        top: offset,
                        behavior: 'smooth'
                    });
                    
                    history.pushState(null, null, targetId);
                }
            }
        });
    });
    
    tocLinks.forEach(link => {
        link.addEventListener('click', function() {
            userClickedLink = this;
            clickTimestamp = Date.now();
        });
    });
}

function setupTooltipAreas() {
    // Create tooltip element if it doesn't exist
    let tooltip = document.getElementById('custom-tooltip');
    if (!tooltip) {
        tooltip = document.createElement('div');
        tooltip.id = 'custom-tooltip';
        tooltip.style.position = 'absolute';
        tooltip.style.backgroundColor = 'rgba(0,0,0,0.8)';
        tooltip.style.color = 'white';
        tooltip.style.padding = '5px 10px';
        tooltip.style.borderRadius = '4px';
        tooltip.style.fontSize = '14px';
        tooltip.style.zIndex = '1000';
        tooltip.style.pointerEvents = 'none';
        tooltip.style.opacity = '0';
        tooltip.style.transition = 'opacity 0.3s';
        document.body.appendChild(tooltip);
    }
    
    // Handle tooltip areas with href attributes
    const tooltipAreasWithHref = document.querySelectorAll('.tooltip-area[href]');
    tooltipAreasWithHref.forEach(area => {
        area.addEventListener('click', function(e) {
            e.preventDefault();
            const targetId = this.getAttribute('href');
            const targetElement = document.querySelector(targetId);
            
            if (targetElement) {
                let contentId = null;
                const parentSection = targetElement.closest('.product-content');
                if (parentSection && parentSection.id) {
                    contentId = parentSection.id.replace('-content', '');
                }
                
                let needsContentSwitch = false;
                if (contentId) {
                    const targetContent = document.getElementById(contentId + '-content');
                    const currentActive = document.querySelector('.product-content.active');
                    
                    if (currentActive !== targetContent) {
                        needsContentSwitch = true;
                        
                        document.querySelectorAll('.product-content').forEach(section => {
                            section.classList.remove('active');
                        });
                        
                        if (targetContent) {
                            targetContent.classList.add('active');
                        }
                        
                        const navLinks = document.querySelectorAll('.product-list a');
                        navLinks.forEach(navLink => {
                            navLink.classList.remove('active');
                            if (navLink.getAttribute('href') === '#' + contentId) {
                                navLink.classList.add('active');
                            }
                        });
                    }
                }
                
                saveCurrentState(contentId, targetId);
                
                if (needsContentSwitch) {
                    setTimeout(() => {
                        const headerHeight = 60;
                        const offset = targetElement.offsetTop - headerHeight;
                        
                        window.scrollTo({
                            top: offset,
                            behavior: 'smooth'
                        });
                    }, 10);
                } else {
                    const headerHeight = 60;
                    const offset = targetElement.offsetTop - headerHeight;
                    
                    window.scrollTo({
                        top: offset,
                        behavior: 'smooth'
                    });
                }
            }
        });
    });
    
    // Handle tooltip display for all tooltip areas
    const tooltipAreas = document.querySelectorAll('.tooltip-area[data-tooltip]');
    tooltipAreas.forEach(area => {
        // Show tooltip on hover
        area.addEventListener('mouseenter', function(e) {
            const tooltipText = this.getAttribute('data-tooltip');
            tooltip.textContent = tooltipText;
            tooltip.style.opacity = '1';
            
            // Position the tooltip near the mouse
            updateTooltipPosition(e);
        });
        
        // Hide tooltip when mouse leaves
        area.addEventListener('mouseleave', function() {
            tooltip.style.opacity = '0';
        });
        
        // Update tooltip position as mouse moves
        area.addEventListener('mousemove', updateTooltipPosition);
    });
    
    function updateTooltipPosition(e) {
        const mouseX = e.clientX;
        const mouseY = e.clientY;
        
        tooltip.style.left = (mouseX + 15) + 'px';
        tooltip.style.top = (mouseY + 15) + 'px';
    }
}

function initializePage() {
    if (window.location.hash) {
        handleHashNavigation(window.location.hash);
    } else {
        const savedState = localStorage.getItem('mach1DocsState');
        if (savedState) {
            try {
                const state = JSON.parse(savedState);
                restoreSavedState(state);
            } catch (e) {
                console.error('Error restoring state:', e);
            }
        }
    }
}

function handleHashNavigation(hash) {
    if (!hash) return;
    
    const tocLink = document.querySelector(`.toc-sidebar a[href="${hash}"]`);
    if (tocLink) {
        const section = tocLink.closest('li.has-submenu');
        if (section) {
            document.querySelectorAll('.toc-sidebar li.has-submenu').forEach(item => {
                item.classList.add('collapsed');
            });
            
            let parent = section;
            while (parent && parent.classList.contains('has-submenu')) {
                parent.classList.remove('collapsed');
                parent = parent.parentElement.closest('li.has-submenu');
            }
        }
        
        const mainLink = section ? section.querySelector(':scope > a') : null;
        if (mainLink) {
            const contentId = mainLink.getAttribute('href').substring(1);
            const contentEl = document.getElementById(contentId + '-content');
            
            if (contentEl) {
                document.querySelectorAll('.product-content').forEach(item => {
                    item.classList.remove('active');
                });
                
                contentEl.classList.add('active');
                
                const targetEl = document.querySelector(hash);
                if (targetEl) {
                    setTimeout(() => {
                        const headerHeight = 60;
                        const offset = targetEl.offsetTop - headerHeight;
                        
                        window.scrollTo({
                            top: offset,
                            behavior: 'auto'
                        });
                    }, 10);
                }
                
                saveCurrentState(contentId, hash);
            }
        }
    }
}

function restoreSavedState(savedState) {
    if (!savedState) return;
    
    if (savedState.contentId) {
        const contentElement = document.getElementById(savedState.contentId + '-content');
        if (contentElement) {
            document.querySelectorAll('.product-content').forEach(item => {
                item.classList.remove('active');
            });
            
            contentElement.classList.add('active');
            
            document.querySelectorAll('.product-list a').forEach(link => {
                link.classList.remove('active');
                if (link.getAttribute('href') === '#' + savedState.contentId) {
                    link.classList.add('active');
                }
            });
            
            document.querySelectorAll('#sidebar .has-submenu').forEach(item => {
                item.classList.add('collapsed');
            });
            
            const sectionLink = document.querySelector(`#sidebar .has-submenu > a[href="#${savedState.contentId}"]`);
            if (sectionLink) {
                const parentLi = sectionLink.closest('.has-submenu');
                if (parentLi) {
                    parentLi.classList.remove('collapsed');
                }
            }
        }
    }
    
    if (savedState.scrollId) {
        const targetElement = document.querySelector(savedState.scrollId);
        if (targetElement) {
            setTimeout(() => {
                const headerHeight = 60;
                const offset = targetElement.offsetTop - headerHeight;
                
                window.scrollTo({
                    top: offset,
                    behavior: 'auto'
                });
                
                const tocLink = document.querySelector(`.toc-sidebar a[href="${savedState.scrollId}"]`);
                if (tocLink) {
                    tocLink.classList.add('active');
                    
                    let parent = tocLink.closest('li.has-submenu');
                    while (parent) {
                        parent.classList.remove('collapsed');
                        parent = parent.parentElement.closest('li.has-submenu');
                    }
                }
            }, 10);
        }
    } else if (savedState.contentId) {
        window.scrollTo({
            top: 0,
            behavior: 'auto'
        });
    }
}

function showDefaultPage() {
    const pannerContent = document.getElementById('panner-content');
    const pannerLink = document.querySelector('.product-list a[href="#panner"]');
    
    document.querySelectorAll('.product-content').forEach(item => {
        item.classList.remove('active');
    });
    
    document.querySelectorAll('.product-list a').forEach(item => {
        item.classList.remove('active');
    });
    
    if (pannerContent) {
        pannerContent.classList.add('active');
    }
    
    if (pannerLink) {
        pannerLink.classList.add('active');
    }
    
    document.querySelectorAll('#sidebar .has-submenu').forEach(item => {
        item.classList.add('collapsed');
    });
    
    const sectionLink = document.querySelector('#sidebar .has-submenu > a[href="#panner"]');
    if (sectionLink) {
        const parentLi = sectionLink.closest('.has-submenu');
        if (parentLi) {
            parentLi.classList.remove('collapsed');
        }
    }
    
    saveCurrentState('panner', null);
}

function setupScrollHighlighting() {
    let scrollTimeout;
    let isScrolling = false;
    let lastUserClickTime = 0;
    
    window.addEventListener('scroll', function() {
        isScrolling = true;
        clearTimeout(scrollTimeout);
        
        // Check if a user just clicked a link - prevent auto highlighting for 1 second
        const userClickedRecently = Date.now() - lastUserClickTime < 1000;
        
        scrollTimeout = setTimeout(() => {
            isScrolling = false;
            
            // Get the most recent user click time
            const tocLink = document.querySelector('.toc-sidebar a.active');
            if (tocLink) {
                const userClickTime = parseInt(tocLink.dataset.lastClicked || '0');
                if (userClickTime > 0) {
                    lastUserClickTime = userClickTime;
                }
            }
            
            // Skip auto highlighting if user clicked recently
            if (!userClickedRecently) {
                highlightCurrentSection();
            }
        }, 100);
    });
    
    // Monitor user clicks on TOC links
    document.querySelectorAll('.toc-sidebar a').forEach(link => {
        link.addEventListener('click', function() {
            const now = Date.now();
            this.dataset.lastClicked = now.toString();
            lastUserClickTime = now;
        });
    });
}

function highlightCurrentSection() {
    // Skip highlighting if user clicked a link within the last 1 second
    if (Date.now() - (parseInt(document.querySelector('.toc-sidebar a.active')?.dataset.lastClicked || '0')) < 1000) {
        return;
    }

    const contentSection = document.querySelector('.product-content.active');
    if (!contentSection) return;
    
    // Get all TOC links for the current content section
    const currentContentId = contentSection.id.replace('-content', '');
    const topLevelItem = document.querySelector(`.toc-sidebar a[href="#${currentContentId}"]`);
    
    if (!topLevelItem) return;
    
    const tocParent = topLevelItem.closest('li.has-submenu');
    const allSectionLinks = Array.from(tocParent.querySelectorAll('a')).filter(link => link.getAttribute('href')?.startsWith('#'));
    
    if (allSectionLinks.length === 0) return;
    
    // Map of all section IDs mentioned in the TOC
    const tocSectionIds = allSectionLinks.map(link => link.getAttribute('href').substring(1));
    
    // Find which section is currently visible
    const scrollPosition = window.scrollY;
    const headerHeight = 100;
    
    // Check if we're at the very top of the page
    if (scrollPosition < 50) {
        // If at top, just highlight the main section
        updateActiveSection(currentContentId);
        return;
    }
    
    // Get all visible sections in the current content section that are also in the TOC
    const sections = Array.from(contentSection.querySelectorAll('[id]')).filter(
        section => tocSectionIds.includes(section.id)
    );
    
    if (sections.length === 0) {
        // If no sections match TOC links, fall back to the main section
        updateActiveSection(currentContentId);
        return;
    }
    
    // Find sections above the current scroll position
    let visibleSections = [];
    for (const section of sections) {
        const sectionTop = section.getBoundingClientRect().top + window.scrollY;
        if (sectionTop < scrollPosition + headerHeight + 150) {
            visibleSections.push({
                id: section.id,
                top: sectionTop
            });
        }
    }
    
    // If no sections are visible, use the main section
    if (visibleSections.length === 0) {
        updateActiveSection(currentContentId);
        return;
    }
    
    // Sort by position (top to bottom) and get the last one (closest to current viewport)
    visibleSections.sort((a, b) => a.top - b.top);
    const currentSection = visibleSections[visibleSections.length - 1];
    
    // Update the active section in the TOC
    updateActiveSection(currentSection.id);
}

function updateActiveSection(sectionId) {
    const linkSelector = `.toc-sidebar a[href="#${sectionId}"]`;
    const activeLink = document.querySelector(linkSelector);
    
    if (activeLink) {
        document.querySelectorAll('.toc-sidebar a').forEach(link => {
            link.classList.remove('active');
        });
        
        activeLink.classList.add('active');
        
        let parent = activeLink.closest('li.has-submenu');
        while (parent) {
            parent.classList.remove('collapsed');
            parent = parent.parentElement.closest('li.has-submenu');
        }
        
        // Only update history if we're not in a click timeout
        if (Date.now() - (parseInt(activeLink.dataset.lastClicked || '0')) > 1000) {
            history.replaceState(null, null, `#${sectionId}`);
        }
        
        const parentContent = document.querySelector(`.product-content.active`);
        if (parentContent && parentContent.id) {
            const contentId = parentContent.id.replace('-content', '');
            saveCurrentState(contentId, `#${sectionId}`);
        }
    }
}

function saveCurrentState(contentId, scrollId) {
    const state = {
        contentId: contentId,
        scrollId: scrollId
    };
    
    localStorage.setItem('mach1DocsState', JSON.stringify(state));
}

function setupBeforeUnloadHandler() {
    window.addEventListener('beforeunload', function() {
        const state = {
            contentId: document.querySelector('.product-content.active')?.id.replace('-content', ''),
            scrollY: window.scrollY,
            scrollId: document.querySelector('.toc-sidebar a.active')?.getAttribute('href')
        };
        
        localStorage.setItem('mach1DocsState', JSON.stringify(state));
    });
}

function setupTopLevelTOCDoubleclicks() {
    const topLevelItems = document.querySelectorAll('.toc-sidebar > .toc-sidebar-inner > ul > li.has-submenu > a');
    
    topLevelItems.forEach(item => {
        let lastClickTime = 0;
        
        item.addEventListener('click', function(e) {
            const now = Date.now();
            const timeSinceLastClick = now - lastClickTime;
            
            if (timeSinceLastClick < 300) {
                e.preventDefault();
                e.stopPropagation();
                
                navigateToPage(this.getAttribute('href').substring(1));
            }
            
            lastClickTime = now;
        });
    });
}

function navigateToPage(contentId) {
    const contentEl = document.getElementById(contentId + '-content');
    
    if (contentEl) {
        document.querySelectorAll('.product-content').forEach(item => {
            item.classList.remove('active');
        });
        
        contentEl.classList.add('active');
        
        document.querySelectorAll('.product-list a').forEach(link => {
            link.classList.remove('active');
            if (link.getAttribute('href') === '#' + contentId) {
                link.classList.add('active');
            }
        });
        
        window.scrollTo({
            top: 0,
            behavior: 'smooth'
        });
        
        saveCurrentState(contentId, null);
        
        history.pushState(null, null, `#${contentId}`);
    }
} 