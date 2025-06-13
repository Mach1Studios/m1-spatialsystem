function handler(event) {
    var request = event.request;
    var uri = request.uri;
    
    // If URI ends with '/' (directory request), append 'index.html'
    if (uri.endsWith('/')) {
        request.uri += 'index.html';
    }
    // If URI doesn't have an extension and doesn't end with '/', 
    // assume it's a directory and append '/index.html'
    else if (!uri.includes('.')) {
        request.uri += '/index.html';
    }
    
    return request;
}