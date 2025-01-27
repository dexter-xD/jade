console.log("Starting test...");
console.error("This is an error message!");

setTimeout(() => {
    console.log("Timeout executed after 1 second!");
}, 1000);

setTimeout(() => {
    console.error("Another error after 2 seconds!");
}, 2000);