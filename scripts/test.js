fs.readFile("/home/dexter/projects/jade/scripts/test.txt", (err, data) => {
    if (err) {
        console.error("Error:", err);
    } else {
        console.log("File Contents:", data);
    }
});
