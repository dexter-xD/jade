console.log("Starting server...");

const server = net.createServer((client) => {
    console.log("New client connected");

    setTimeout(() => {
        client.write("Hello from server!\n");
    }, 2000);
});

server.listen(8080);
console.log("Server running on port 8080");
