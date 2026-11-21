const screens = [...document.querySelectorAll('#firmware-screens figure')];
const controls = document.querySelector('.screen-controls');
const [previous, next] = controls.querySelectorAll('button');
let current = 0;

function move(step) {
  screens[current].hidden = true;
  current = (current + step + screens.length) % screens.length;
  screens[current].hidden = false;
}

previous.addEventListener('click', () => move(-1));
next.addEventListener('click', () => move(1));
controls.hidden = false;
