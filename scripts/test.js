http.get("http://httpbin.org/json", (err, data) => {
    if (err) {
        console.error("Error:", err);
    } else {
        console.log("Response:", data);  
    }
});
