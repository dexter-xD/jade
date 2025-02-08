fs.readFile("/home/dexter/projects/jade/scripts/test.txt", (err, data) => {
    if (err) {
        console.error("Error:", err);
    } else {
        console.log("File Contents:", data);
    }
});

fs.writeFile("output.txt", "Hello from C runtime!", (err) => {
    if (err) {
        console.error("Write Error:", err);
    } else {
        console.log("File written successfully!");
    }
});
