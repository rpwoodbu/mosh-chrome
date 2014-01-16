chrome.app.runtime.onLaunched.addListener(function() {
  chrome.app.window.create('mosh_client.html', {
    'id': 'mosh_client',
  });
});
