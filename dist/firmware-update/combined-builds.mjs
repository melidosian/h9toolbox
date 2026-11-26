const builds = {
  '0.1.6': [
    {
      id: '0.1.6-rc.4', file: 'dev/h9-toolbox-firmware-v0.1.6-rc.4.fw',
      packageSha256: '8e7cb1fc97605704665bb7fa0add10fb83ef4c0950189e0920200193a6e189ce',
      imageSha256: '7589d63f7b7eee83908a00852cfef0538b920364d71e0e532085ab62ae34fc43',
      farFile: 'dev/h9-toolbox-v0.1.6-rc.4-far.bin',
      farSha256: '4dfa6622ac375a54f470fba6d51cb07f60986cf5796b1b05a7cf3eddf4e89f26',
    },
    {
      id: '0.1.6-rc.5', file: 'dev/h9-toolbox-firmware-v0.1.6-rc.5.fw',
      packageSha256: '395fbc94aeca54eb130b0bf811107bdf06b7c524972b0ca064e26a827a7b3c76',
      imageSha256: 'fa79bef5fa13e6cbaccd0d4b39eb7a24b51b2ef565ea84503e2e7d08e1e24ac0',
      farFile: 'dev/h9-toolbox-v0.1.6-rc.5-far.bin',
      farSha256: '9d1b7ec6b72d7aee88183b08e21c9604bd15ad1b6451868471cbd86d64fa7aa0',
    },
  ],
  '0.1.7': [
    {
      id: '0.1.7-no-modem', file: 'dev/h9-toolbox-firmware-v0.1.7-no-modem.fw',
      packageSha256: '2ceb391e826eec1661edc0ab985ce7e566fce80a872d24a94bbd48a1eaec4009',
      imageSha256: '17f92514ac9efb3f8d61ce5792c4a36a1ab00618cd38442fee622e7bfb6c599b',
      farFile: 'dev/h9-toolbox-v0.1.7-no-modem-far.bin',
      farSha256: 'e7e0670ad0d39e8c35eab167c1dbe973314df2cfc7216550e39c92fd3f2fd71a',
    },
    {
      id: '0.1.7', file: 'dev/h9-toolbox-firmware-v0.1.7.fw',
      packageSha256: 'fff743efb96abab825cebb5f1dd997702c9aeab978f36545b71c326a48ea8748',
      imageSha256: '356822ff23363724e0ac09b6f966d2eba58344e4ce8a9c5ff23bcf55a106777c',
      farFile: 'dev/h9-toolbox-v0.1.7-far.bin',
      farSha256: 'fcb9039e197d53849b6b976eec2128178255cf8c5ca899041b8c4bd7fc04c8d4',
    },
  ],
  '0.1.8': [
    {
      id: '0.1.8-no-modem', file: 'dev/h9-toolbox-firmware-v0.1.8-no-modem.fw',
      packageSha256: '7f6c523b729cadb2daf97c8322191c42fbbc091f9529148e00339db261f064e6',
      imageSha256: '710ca9056297a1468ac631e564d90c3621e2035cf1c0a3cab7adaa6bfe52af50',
      farFile: 'dev/h9-toolbox-v0.1.8-no-modem-far.bin',
      farSha256: '30bda7d48b2c3e9b3221fc500fc87dcf0dfafa1d263bb7eb43074e0052bcf450',
    },
    {
      id: '0.1.8', file: 'dev/h9-toolbox-firmware-v0.1.8.fw',
      packageSha256: 'f63eb09cb7b7ece0134f85be28ff3a79b78874346b995c7827bc709295af15a6',
      imageSha256: 'b238c0384d6425da590efa3ff09f791eae84b6b4333743de325ae9fbe07a4ab4',
      farFile: 'dev/h9-toolbox-v0.1.8-far.bin',
      farSha256: '263707b65e149f016d3144951711c4ecb38fc205094b322f96f21fd1507063d1',
    },
  ],
};

export function selectCombinedBuild({ quickControls = false, band33cm = false, includeModem = true, dev = false } = {}) {
  if (band33cm && (!quickControls || !dev)) throw new Error('The 33 cm option requires the development installer and Quick Change.');
  const version = band33cm ? '0.1.8' : quickControls ? '0.1.7' : '0.1.6';
  return {
    ...builds[version][Number(includeModem)],
    version,
    catalog: version,
    label: `H9 Toolbox Firmware v${version}${includeModem ? '' : ' without modem updater'}`,
  };
}

export function availableCombinedBuilds(dev) {
  const options = [{}, { quickControls: true }];
  if (dev) options.push({ quickControls: true, band33cm: true, dev });
  return options.flatMap(option => [true, false].map(includeModem => selectCombinedBuild({ ...option, includeModem })));
}
