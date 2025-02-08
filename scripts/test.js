console.log("Starting server...");

const server = net.createServer((client) => {
    console.log("New client connected:", client);
});

server.listen(8080);
console.log("Server running on port 8080");
