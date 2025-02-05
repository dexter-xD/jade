// Test process.argv
console.log("PROCESS TEST: Arguments:", process.argv);

// Test process.exit
setTimeout(() => {
    console.log("PROCESS TEST: Exiting with code 42");
    process.exit(42);
}, 1000);