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
    const tooltipAreas = document.querySelectorAll('.tooltip-area[href]');
    tooltipAreas.forEach(area => {
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
    window.addEventListener('scroll', highlightCurrentSection);
}

function isElementInViewport(el) {
    const rect = el.getBoundingClientRect();
    const headerHeight = 100;
    
    return (
        rect.top >= headerHeight &&
        rect.top <= (window.innerHeight || document.documentElement.clientHeight) / 3
    );
}

function highlightCurrentSection() {
    const contentSection = document.querySelector('.product-content.active');
    if (!contentSection) return;
    
    const sections = Array.from(contentSection.querySelectorAll('.doc-section, .doc-subsection'));
    
    let currentSection = null;
    
    for (let i = 0; i < sections.length; i++) {
        if (isElementInViewport(sections[i])) {
            currentSection = sections[i];
            break;
        }
    }
    
    if (!currentSection && sections.length > 0) {
        const scrollPos = window.scrollY || document.documentElement.scrollTop;
        
        if (scrollPos < 50) {
            currentSection = sections[0];
        } else {
            let closestSection = null;
            let closestDistance = Infinity;
            
            sections.forEach(section => {
                const rect = section.getBoundingClientRect();
                const distance = Math.abs(rect.top);
                
                if (distance < closestDistance) {
                    closestDistance = distance;
                    closestSection = section;
                }
            });
            
            currentSection = closestSection;
        }
    }
    
    if (currentSection) {
        const sectionId = currentSection.id;
        if (sectionId) {
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
                
                history.replaceState(null, null, `#${sectionId}`);
                
                const parentContent = currentSection.closest('.product-content');
                if (parentContent && parentContent.id) {
                    const contentId = parentContent.id.replace('-content', '');
                    saveCurrentState(contentId, `#${sectionId}`);
                }
            }
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