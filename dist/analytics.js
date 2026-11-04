if (location.protocol === 'https:' && location.hostname === 'h9toolbox.com') {
  const measurementId = 'G-RY9PD879YS';
  window.dataLayer = window.dataLayer || [];
  window.gtag = function () { window.dataLayer.push(arguments); };
  window.gtag('js', new Date());
  window.gtag('config', measurementId, {
    page_location: 'https://h9toolbox.com/',
    page_title: 'H9 Toolbox',
    send_page_view: true,
    allow_google_signals: false,
    allow_ad_personalization_signals: false,
  });

  const script = document.createElement('script');
  script.async = true;
  script.src = `https://www.googletagmanager.com/gtag/js?id=${measurementId}`;
  document.head.appendChild(script);
}
