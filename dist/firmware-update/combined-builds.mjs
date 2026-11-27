const builds = {
  '0.1.6': [
    {
      id: '0.1.6-rc.4', file: 'dev/h9-toolbox-firmware-v0.1.6-rc.4.fw',
      packageSha256: 'ef8f1cf378ed0b7d54571050b8b5797e1d0a32e1056555fe310bb511f57f8112',
      imageSha256: '8750726fff64f7f08d2d7eb3c92ababb1918b7ede38627e04b54ba2a922114b2',
      farFile: 'dev/h9-toolbox-v0.1.6-rc.4-far.bin',
      farSha256: '2f8b0dff994a61e9e0df8dfc821545f3614eaf65687196e56e9f6ec23632bc01',
    },
    {
      id: '0.1.6-rc.5', file: 'dev/h9-toolbox-firmware-v0.1.6-rc.5.fw',
      packageSha256: 'cb1bc6869a117342c903f88cef851bcc705d449a2d12e7755f4e0b2f7504b70d',
      imageSha256: '3d51a00b0db5f27e0013aab4d98139326dcf03b736741a71dbb10b83e81c2810',
      farFile: 'dev/h9-toolbox-v0.1.6-rc.5-far.bin',
      farSha256: '7da26507a9937ea6a17efcb4d9d0ee6b6cc9b918e9ead1590c8bd918fd2c1c91',
    },
  ],
  '0.1.7': [
    {
      id: '0.1.7-no-modem', file: 'dev/h9-toolbox-firmware-v0.1.7-no-modem.fw',
      packageSha256: '90986fc4cca5b303dcfd945b8fe13d4bb21b71661639d75a3550371c768938be',
      imageSha256: '86e11b7de905e66e95b94cea7639b18c11858f73d2022360fc4f6b9ccd763fde',
      farFile: 'dev/h9-toolbox-v0.1.7-no-modem-far.bin',
      farSha256: '01dec477daa9d6cab990e815b08ffac131da0cf3a6925783fb671ff5a4fee375',
    },
    {
      id: '0.1.7', file: 'dev/h9-toolbox-firmware-v0.1.7.fw',
      packageSha256: '39b00c2bd0307b64cc6026d3da820e34c5fe719153e6f696f7d9830f68656d68',
      imageSha256: 'e14268ad9109a5ab63d5fdefdb41898aebcf4586c3c62bb7f6725a81fea545e4',
      farFile: 'dev/h9-toolbox-v0.1.7-far.bin',
      farSha256: 'e52d7451fde10bcad2c7d861f85d845031b04fbe66f71e51ae05ea19bee5b558',
    },
  ],
  '0.1.8': [
    {
      id: '0.1.8-no-modem', file: 'dev/h9-toolbox-firmware-v0.1.8-no-modem.fw',
      packageSha256: '9d229128c1454638de535fb5b447bbf2ae71aa73b341585f9da2a7d4e3f2d322',
      imageSha256: '4ba7fc607998d126827bc5e0eff9c5d64a1daae324c0e4bc2f670cf0d888a2c3',
      farFile: 'dev/h9-toolbox-v0.1.8-no-modem-far.bin',
      farSha256: '3e613541be52ebfe1d3974b78069f21c96630bcac2d71bb4efd8eb7f1e6ffc15',
    },
    {
      id: '0.1.8', file: 'dev/h9-toolbox-firmware-v0.1.8.fw',
      packageSha256: '7678bde01ebc596b6b9fa1b5de76f966343c7d64ea447aef6b04513b80961714',
      imageSha256: 'b86506d122d2a2c80b9b09168a53d87bf3dde84a6806e5a1502ace242a2f68f1',
      farFile: 'dev/h9-toolbox-v0.1.8-far.bin',
      farSha256: '0a171b874a01b448f1897169652f13315f668b10ae4ed88111fb745fd7b3a865',
    },
  ],
};

export function selectCombinedBuild({ quickControls = false, band33cm = false, includeModem = true, allow33cm = false } = {}) {
  if (band33cm && !allow33cm) throw new Error('Experimental 33 cm controls are unavailable in this firmware release.');
  if (band33cm && !quickControls) throw new Error('Experimental 33 cm controls require Quick Change.');
  const version = band33cm ? '0.1.8' : quickControls ? '0.1.7' : '0.1.6';
  return {
    ...builds[version][Number(includeModem)],
    version,
    catalog: version,
    label: `H9 Toolbox Firmware v${version}${includeModem ? '' : ' without modem updater'}`,
  };
}

export function availableCombinedBuilds(allow33cm) {
  const options = [{}, { quickControls: true }];
  if (allow33cm) options.push({ quickControls: true, band33cm: true, allow33cm });
  return options.flatMap(option => [true, false].map(includeModem => selectCombinedBuild({ ...option, includeModem })));
}
