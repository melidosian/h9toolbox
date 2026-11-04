if (location.protocol === 'http:' && ['h9toolbox.com', 'www.h9toolbox.com'].includes(location.hostname)) {
  location.replace(`https://h9toolbox.com${location.pathname}${location.search}${location.hash}`);
}
