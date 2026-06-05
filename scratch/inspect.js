const { app } = require('electron');

function inner() {
  console.log('Stack trace:\n', new Error().stack);
}

function outer() {
  inner();
}

app.whenReady().then(() => {
  outer();
  app.quit();
});

