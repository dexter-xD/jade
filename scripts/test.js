let count = 0;
const id = setInterval(() => {
    console.log("Tick", ++count);
    if (count >= 5) clearInterval(id);
}, 1000);