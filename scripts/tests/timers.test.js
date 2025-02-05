// Test setTimeout/clearTimeout
let timeoutRan = false;
const timeoutId = setTimeout(() => {
    timeoutRan = true;
    console.log("TIMER TEST: Timeout executed");
}, 500);
clearTimeout(timeoutId);

// Test setInterval/clearInterval
let intervalCount = 0;
const intervalId = setInterval(() => {
    intervalCount++;
    console.log(`TIMER TEST: Interval ${intervalCount}`);
    if (intervalCount >= 3) {
        clearInterval(intervalId);
        console.log("TIMER TEST: Interval cleared");
    }
}, 200);