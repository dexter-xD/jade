const server = http.createServer((req, res) => {
    console.log(`Received ${req.method} request for ${req.url}`);
    res.end();
});

server.listen(8080);
console.log("Server running on port 8080");