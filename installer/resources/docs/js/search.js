/**
 * Search functionality for Mach1 Documentation
 */

function setupSearch() {
    const searchContainer = document.getElementById('search-container');
    if (!searchContainer) return;

    const searchInput = searchContainer.querySelector('#search-input');
    const searchResults = searchContainer.querySelector('#search-results');
    const clearButton = searchContainer.querySelector('#clear-search');

    // Index all content for search
    const searchIndex = buildSearchIndex();

    searchInput.addEventListener('input', function() {
        const query = this.value.trim().toLowerCase();
        
        if (query.length < 2) {
            searchResults.innerHTML = '';
            searchResults.classList.remove('active');
            return;
        }

        const results = performSearch(query, searchIndex);
        displaySearchResults(results, searchResults);
    });

    clearButton.addEventListener('click', function() {
        searchInput.value = '';
        searchResults.innerHTML = '';
        searchResults.classList.remove('active');
        searchInput.focus();
    });

    // Close search results when clicking outside
    document.addEventListener('click', function(e) {
        if (!searchContainer.contains(e.target)) {
            searchResults.classList.remove('active');
        }
    });
}

function buildSearchIndex() {
    const index = [];
    
    // Index all section headings and content
    document.querySelectorAll('.product-content h2, .product-content h3, .product-content h4, .doc-section, .doc-subsection').forEach(el => {
        const id = el.id;
        if (!id) return;
        
        const text = el.textContent;
        const type = el.tagName.toLowerCase();
        const parentContent = el.closest('.product-content');
        const section = parentContent ? parentContent.id.replace('-content', '') : '';
        
        // Add the heading itself
        index.push({
            id: id,
            text: text,
            type: type,
            section: section
        });
        
        // Add paragraphs within sections for more detailed search
        if (type === 'div') {
            el.querySelectorAll('p').forEach((p, i) => {
                index.push({
                    id: id,
                    text: p.textContent,
                    type: 'paragraph',
                    section: section,
                    paragraphIndex: i
                });
            });
        }
    });
    
    return index;
}

function performSearch(query, index) {
    const results = [];
    const terms = query.split(' ').filter(term => term.length > 1);
    
    index.forEach(item => {
        let score = 0;
        const text = item.text.toLowerCase();
        
        // Check for exact match
        if (text.includes(query)) {
            score += 10;
        }
        
        // Check for term matches
        terms.forEach(term => {
            if (text.includes(term)) {
                score += 5;
            }
        });
        
        // Boost score for headings
        if (item.type === 'h2') score *= 3;
        if (item.type === 'h3') score *= 2;
        if (item.type === 'h4') score *= 1.5;
        
        if (score > 0) {
            results.push({
                ...item,
                score: score
            });
        }
    });
    
    // Sort by score (highest first)
    results.sort((a, b) => b.score - a.score);
    
    // Limit to top 10 results
    return results.slice(0, 10);
}

function displaySearchResults(results, resultsContainer) {
    if (results.length === 0) {
        resultsContainer.innerHTML = '<div class="no-results">No results found</div>';
        resultsContainer.classList.add('active');
        return;
    }
    
    let html = '';
    
    // Group results by section
    const sectionMap = {};
    results.forEach(result => {
        if (!sectionMap[result.section]) {
            sectionMap[result.section] = [];
        }
        sectionMap[result.section].push(result);
    });
    
    // Generate HTML for each section
    Object.keys(sectionMap).forEach(section => {
        const sectionResults = sectionMap[section];
        const sectionName = section.charAt(0).toUpperCase() + section.slice(1);
        
        html += `<div class="search-section">
            <div class="search-section-title">${sectionName}</div>
            <ul>`;
        
        sectionResults.forEach(result => {
            let displayText = result.text;
            // Truncate long text
            if (displayText.length > 60) {
                displayText = displayText.substring(0, 57) + '...';
            }
            
            html += `<li>
                <a href="#${result.id}" class="search-result" data-section="${result.section}">
                    <span class="result-text">${displayText}</span>
                    <span class="result-type">${getTypeLabel(result.type)}</span>
                </a>
            </li>`;
        });
        
        html += `</ul></div>`;
    });
    
    resultsContainer.innerHTML = html;
    resultsContainer.classList.add('active');
    
    // Add click handlers to results
    resultsContainer.querySelectorAll('.search-result').forEach(link => {
        link.addEventListener('click', function(e) {
            e.preventDefault();
            
            const targetId = this.getAttribute('href');
            const section = this.getAttribute('data-section');
            
            // Switch to the correct section first
            navigateToPage(section);
            
            // Then scroll to the specific element
            setTimeout(() => {
                const targetElement = document.querySelector(targetId);
                if (targetElement) {
                    const headerHeight = 60;
                    const offset = targetElement.offsetTop - headerHeight;
                    
                    window.scrollTo({
                        top: offset,
                        behavior: 'smooth'
                    });
                    
                    // Highlight the element briefly
                    targetElement.classList.add('search-highlight');
                    setTimeout(() => {
                        targetElement.classList.remove('search-highlight');
                    }, 2000);
                }
                
                // Clear search
                resultsContainer.innerHTML = '';
                resultsContainer.classList.remove('active');
                document.getElementById('search-input').value = '';
            }, 100);
        });
    });
}

function getTypeLabel(type) {
    switch(type) {
        case 'h2': return 'Title';
        case 'h3': return 'Section';
        case 'h4': return 'Subsection';
        case 'div': return 'Content';
        case 'paragraph': return 'Text';
        default: return type;
    }
}

// Initialize search when DOM is loaded
document.addEventListener('DOMContentLoaded', setupSearch); 