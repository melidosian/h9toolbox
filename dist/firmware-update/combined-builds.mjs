const builds = {
  '0.1.6': [
    {
      id: '0.1.6-rc.4', file: 'dev/h9-toolbox-firmware-v0.1.6-rc.4.fw',
      packageSha256: 'fd5caff805b3faecb660835d210d54fe7f49efc4787b37afe71858a4467ebef9',
      imageSha256: '5a9b59346d01b33fb6d345d53afce5d633dc96af1ffd53c3b69fd732b4409342',
      farFile: 'dev/h9-toolbox-v0.1.6-rc.4-far.bin',
      farSha256: 'a31c029ca64bd18cf3a337320f77dd9fcb2925c742d2cf2bba08a3bc4b93896e',
    },
    {
      id: '0.1.6-rc.5', file: 'dev/h9-toolbox-firmware-v0.1.6-rc.5.fw',
      packageSha256: '95c30fcce4b411c3e7d82db2761e39c13818a5cf62d6ff87cc5c41f2c16cbe36',
      imageSha256: 'ef94b9b45082c152bfcc7b21e9f20f4cdb79b5a43e9f214fc983ddfacd358698',
      farFile: 'dev/h9-toolbox-v0.1.6-rc.5-far.bin',
      farSha256: '5e1e65f19e88d30c00f304bd19193968ec4f2ecdcf1395137b0e5ae9f7ab871d',
    },
  ],
  '0.1.7': [
    {
      id: '0.1.7-no-modem', file: 'dev/h9-toolbox-firmware-v0.1.7-no-modem.fw',
      packageSha256: '51abd446156565b1c29ce1f4fe5b1f3254ae289956267b6613c53ad8dbdc2725',
      imageSha256: 'ef40abc554055db8ca7aeac9061d495a5c6ea9fd64ba2dfd7b18a36c591523b6',
      farFile: 'dev/h9-toolbox-v0.1.7-no-modem-far.bin',
      farSha256: '3ee7e0aa70d8fb84e643c924663d05e9f64b0cb0b34f674b7389d4eddbba4b20',
    },
    {
      id: '0.1.7', file: 'dev/h9-toolbox-firmware-v0.1.7.fw',
      packageSha256: 'd0961229c589de90627c58eb31c376198bd29008fb069f96537f857de5d448a5',
      imageSha256: '05a0c765e7ab8a42646b1a6cbe194d5f6244b632986c4bbea1a6ce3b92aeb090',
      farFile: 'dev/h9-toolbox-v0.1.7-far.bin',
      farSha256: '460f01968847e9d9f500d8cfdd70807bf1a8faabf2b44415d89920f8f0f2c05f',
    },
  ],
  '0.1.8': [
    {
      id: '0.1.8-no-modem', file: 'dev/h9-toolbox-firmware-v0.1.8-no-modem.fw',
      packageSha256: 'e11e1d827586584e01f9d81ca17b41f70e31bbd59be79d9d6ad5b48974d1c481',
      imageSha256: '7d9ee46529e7fb2cdb9d5383487b076ee28a55ba15b59cf00892f1e937a6c078',
      farFile: 'dev/h9-toolbox-v0.1.8-no-modem-far.bin',
      farSha256: '07d963470f320980836245e6c0b6ab3b92546f51ce15479aa5574fb07ca79b32',
    },
    {
      id: '0.1.8', file: 'dev/h9-toolbox-firmware-v0.1.8.fw',
      packageSha256: 'e1d327cc7a8bff81ddb621d13320dcc52f456d5bbdebddff8de89f731138a372',
      imageSha256: '0d83de621d41256ef41c48c12877e75b4530c54041a67384cdfa937ef5476db5',
      farFile: 'dev/h9-toolbox-v0.1.8-far.bin',
      farSha256: 'f44876f4826a424a4ed19b76fdbd07f74ff57ca1d41eecf4351c90f3b3014314',
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
