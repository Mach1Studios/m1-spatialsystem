/**
 * Print/PDF Export functionality for Mach1 Documentation
 */

function setupPrintFunctionality() {
    const printButton = document.getElementById('print-button');
    if (!printButton) return;
    
    printButton.addEventListener('click', function() {
        // Show print options dialog
        showPrintOptions();
    });
}

function showPrintOptions() {
    // Create modal dialog
    const modal = document.createElement('div');
    modal.className = 'print-modal';
    modal.innerHTML = `
        <div class="print-modal-content">
            <div class="print-modal-header">
                <h3>Print or Export as PDF</h3>
                <button class="print-modal-close">&times;</button>
            </div>
            <div class="print-modal-body">
                <p>Choose what to print:</p>
                
                <div class="print-option">
                    <input type="radio" id="print-current" name="print-option" value="current" checked>
                    <label for="print-current">Current section only</label>
                </div>
                
                <div class="print-option">
                    <input type="radio" id="print-all" name="print-option" value="all">
                    <label for="print-all">All documentation</label>
                </div>
                
                <div class="print-option">
                    <input type="radio" id="print-custom" name="print-option" value="custom">
                    <label for="print-custom">Select sections:</label>
                </div>
                
                <div id="custom-sections" class="custom-sections">
                    <div class="print-checkbox">
                        <input type="checkbox" id="print-panner" name="print-section" value="panner">
                        <label for="print-panner">Mach1 Panner</label>
                    </div>
                    <div class="print-checkbox">
                        <input type="checkbox" id="print-monitor" name="print-section" value="monitor">
                        <label for="print-monitor">Mach1 Monitor</label>
                    </div>
                    <div class="print-checkbox">
                        <input type="checkbox" id="print-player" name="print-section" value="player">
                        <label for="print-player">Mach1 Player</label>
                    </div>
                    <div class="print-checkbox">
                        <input type="checkbox" id="print-transcoder" name="print-section" value="transcoder">
                        <label for="print-transcoder">Mach1 Transcoder</label>
                    </div>
                    <div class="print-checkbox">
                        <input type="checkbox" id="print-guide" name="print-section" value="guide">
                        <label for="print-guide">User Guide</label>
                    </div>
                </div>
                
                <div class="print-tips">
                    <p><strong>Tips for PDF export:</strong></p>
                    <ul>
                        <li>Use your browser's print dialog and select "Save as PDF"</li>
                        <li>For best results, use Chrome or Edge</li>
                        <li>Set margins to "None" for maximum content width</li>
                    </ul>
                </div>
            </div>
            <div class="print-modal-footer">
                <button id="cancel-print" class="btn">Cancel</button>
                <button id="confirm-print" class="btn btn-primary">Print / Export PDF</button>
            </div>
        </div>
    `;
    
    document.body.appendChild(modal);
    
    // Add event listeners
    const closeButton = modal.querySelector('.print-modal-close');
    const cancelButton = modal.querySelector('#cancel-print');
    const confirmButton = modal.querySelector('#confirm-print');
    const customSections = modal.querySelector('#custom-sections');
    const printOptions = modal.querySelectorAll('input[name="print-option"]');
    
    closeButton.addEventListener('click', () => {
        document.body.removeChild(modal);
    });
    
    cancelButton.addEventListener('click', () => {
        document.body.removeChild(modal);
    });
    
    // Toggle custom sections visibility
    printOptions.forEach(option => {
        option.addEventListener('change', function() {
            if (this.value === 'custom') {
                customSections.style.display = 'block';
            } else {
                customSections.style.display = 'none';
            }
        });
    });
    
    // Initial state
    customSections.style.display = 'none';
    
    // Print action
    confirmButton.addEventListener('click', () => {
        const selectedOption = modal.querySelector('input[name="print-option"]:checked').value;
        
        // Prepare for printing
        const originalActiveContent = document.querySelector('.product-content.active');
        const originalActiveId = originalActiveContent ? originalActiveContent.id : null;
        
        // Hide all content first
        document.querySelectorAll('.product-content').forEach(content => {
            content.classList.remove('active');
            content.style.display = 'none';
        });
        
        if (selectedOption === 'current') {
            // Print only current section
            if (originalActiveId) {
                const contentToShow = document.getElementById(originalActiveId);
                contentToShow.classList.add('active');
                contentToShow.style.display = 'block';
            }
        } else if (selectedOption === 'all') {
            // Print all sections
            document.querySelectorAll('.product-content').forEach(content => {
                content.classList.add('active');
                content.style.display = 'block';
            });
        } else if (selectedOption === 'custom') {
            // Print selected sections
            const selectedSections = modal.querySelectorAll('input[name="print-section"]:checked');
            selectedSections.forEach(section => {
                const contentId = section.value + '-content';
                const contentToShow = document.getElementById(contentId);
                if (contentToShow) {
                    contentToShow.classList.add('active');
                    contentToShow.style.display = 'block';
                }
            });
        }
        
        // Add print-specific class to body
        document.body.classList.add('printing');
        
        // Remove modal
        document.body.removeChild(modal);
        
        // Trigger print
        setTimeout(() => {
            window.print();
            
            // Restore original state after print dialog closes
            setTimeout(() => {
                // Remove print class
                document.body.classList.remove('printing');
                
                // Hide all content again
                document.querySelectorAll('.product-content').forEach(content => {
                    content.classList.remove('active');
                    content.style.display = 'none';
                });
                
                // Restore original active content
                if (originalActiveId) {
                    const originalContent = document.getElementById(originalActiveId);
                    if (originalContent) {
                        originalContent.classList.add('active');
                        originalContent.style.display = 'block';
                    }
                }
            }, 1000);
        }, 200);
    });
}

// Initialize print functionality when DOM is loaded
document.addEventListener('DOMContentLoaded', setupPrintFunctionality); 