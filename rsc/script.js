fetch("/upload", {
    method: 'POST',
    body: 'Hello, Javascript!'
}).then(res => res.text())
    .then(console.log);
