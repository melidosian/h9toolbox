import { mountLogoTool } from './logo-tool.mjs?v=20261108b';

const $ = id => document.getElementById(id);
function status(message, error = false) {
  $('status').textContent = message;
  $('status').classList.toggle('error-status', error);
  const item = document.createElement('li'); item.textContent = message; $('log').prepend(item);
}
mountLogoTool($('logoTool'), { onStatus: status });
if (!navigator.bluetooth) status('Web Bluetooth is unavailable here. Use desktop Chrome or Edge over HTTPS.', true);
