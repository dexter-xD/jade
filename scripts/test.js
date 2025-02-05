const id = setTimeout(() => {
    console.log("This should not run");
}, 1000);

clearTimeout(id);
console.log("Timeout cleared");