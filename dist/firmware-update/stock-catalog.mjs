// Type-C vendor releases mirrored at the pinned TDH9 archive revision.
const archiveRoot = 'https://raw.githubusercontent.com/teedubyeah/TDH9/1ccb67c2684204b0097533486c545607caaf65b8/Firmware/';

export const STOCK_SIGNATURE_SECTORS = [0x12000, 0x36000, 0x70000, 0xa0000, 0xb6000];
export const STOCK_RELEASES = [
  {
    version: '1.0.23', archive: 'H9 Update-Type-C_1.0.23.zip',
    zipBytes: 7186907, zipSha256: 'e38f557934e68d011009ddd2cb8c0e6e4b4229cd972751c989904adc30d1c291',
    path: 'TID-H9-1.0.23.fw', packageBytes: 1097568,
    packageSha256: '950cca2dea5bd8270a82e0f05951e7a93e85b5d5030b82ee5918df5214abd897',
    imageBytes: 0xc5000, imageSha256: 'd0f2b6fd48f0129241e51926a887aaf2db3b7a1bbc374dbc7250ed487e0eb6aa',
    signature: [0xd203, 0x2957, 0x310d, 0x60a8, 0xbbad],
  },
  {
    version: '1.0.24', archive: 'H9_Update-Type-C_1.0.24.zip',
    zipBytes: 7234700, zipSha256: '2531922ecbbade6e75325e42445864de564d0465c62ca06c76ef292958b36e77',
    path: 'TD-H9-V1.0.24.fw', packageBytes: 1097568,
    packageSha256: 'f95cf6c89ee4e9a70e1d4ab4d4d009155c5e78435daec17d1cb3b7b8555e6ecd',
    imageBytes: 0xc5000, imageSha256: '1ac5f1d6fdc964d7c43cabd2363c21f361c1359b6aafa85e5fc2f0a3d52d941b',
    signature: [0x31a7, 0x0322, 0xe7bf, 0x266f, 0xaed7],
  },
  {
    version: '1.0.27', archive: 'H9_firmware_1.0.27.zip',
    zipBytes: 7228455, zipSha256: '4e64e26bbcd79fb0acaaf92d039353ad80d5c1f918b54fe2bc98c780c3259e61',
    path: 'TD-H9-1.0.27.fw', packageBytes: 1093472,
    packageSha256: '64bc11ac63c60211f64269133ac425b62badf4e41db1ac2b3462b59691b0e838',
    imageBytes: 0xc4000, imageSha256: '41759f4b9ef48113e39c70599adc0fe9d70c1f63f8a06ee5f6d0d84e1f11d5c4',
    signature: [0xe1d5, 0xd5c9, 0x479f, 0x2459, 0x01b7],
    outer: {
      path: 'H9 Update-Type-C.zip', bytes: 16064483,
      sha256: '07dba334b19c3ba3f2608453b80b8bbac49e46bc9e46c48e54acba878fda7213',
    },
  },
  {
    version: '1.0.28', archive: 'H9_Update-Type-C_1.0.28.zip',
    zipBytes: 7228319, zipSha256: '1f1a3bc86dda465b4ea07ecf5ddcd1c1276a98b402efa858a92e966063ffd385',
    path: 'TD-H9-1.0.28.fw', packageBytes: 1093472,
    packageSha256: 'a468bc4507ff44bd4264efec88ab8c38a3ef657e28b087f1a9d07ee4026bbdce',
    imageBytes: 0xc4000, imageSha256: '75f85fc276a332e8d5b06bf1690367bdb5acffef29b6276c3fe3c51ba8f19477',
    signature: [0x22fa, 0xc2fd, 0x586b, 0xe0c2, 0xd918],
  },
  {
    version: '1.0.29', archive: 'H9_Update-Type-C_1.0.29.zip',
    zipBytes: 7228413, zipSha256: 'd9e07efd98e395bb5fc4ccd59405164a8ed8ab7e3775cd35d8395e3ca6e8468e',
    path: 'TD-H9-1.0.29.fw', packageBytes: 1093472,
    packageSha256: '540315655893d04e6fda3e34ebc908e6cb164a76bc6893f2ffba8089be918277',
    imageBytes: 0xc4000, imageSha256: '73c920b5f4b3e6af297ef47cf6fb9c4baf91ad5bf8617fa863a7040c4cf7b959',
    signature: [0x3081, 0x0f9e, 0x10f4, 0x639b, 0x0c5c],
  },
  {
    version: '1.0.32', archive: 'H9_Update-Type-C_1.0.32.zip',
    zipBytes: 7246813, zipSha256: '383d76ba0ee23c8b81efea8eaade319f87a123d48854bf5091ab3d94acc61440',
    path: 'TD-H9-V1.0.32.fw', packageBytes: 1109856,
    packageSha256: '01624e122c53103c2a4440f5e3bdfabcb5280fe08d9543c54b038ceec5f46112',
    imageBytes: 0xc8000, imageSha256: '5049cf60f73eac683c560e21662d0430bac1c6f88f2e1067131962570a1c961f',
    signature: [0x58bc, 0x71c4, 0x85d5, 0x3f70, 0x8803],
  },
  {
    version: '1.0.33', archive: 'H9_Update-Type-C_1.0.33.zip',
    zipBytes: 7081310, zipSha256: 'df6419c53fb318df4830dfed417ed8e3be94f546c5a6093ae9ec47be7c8edf9c',
    path: 'TD-H9-V1.0.33.fw', packageBytes: 881056,
    packageSha256: '288113dd0d1f7156f9a5485b5d196868eeddd949b826dbb11863da824f529060',
    imageBytes: 0xd6000, imageSha256: 'd0df607441a44765127cd5f00e34398f5866712dff3dfb17dc6f1fbf81118dc0',
    signature: [0x873a, 0x4170, 0xaa0c, 0x6cf1, 0x4591],
  },
  {
    version: '1.0.38', archive: 'H9_Update-Type-C_1.0.38.zip',
    zipBytes: 7091957, zipSha256: '1d1b6f87705f026dc5c5b118786d133cbc7f0589f606c8572581df981492155a',
    path: 'H9_Update-Type-C/TD-H9-V1.0.38.fw', packageBytes: 897440,
    packageSha256: 'c4d0beb5281f3a06d7a56083e1744b925f78ff9917c8364e006418ff0bbd0cd6',
    imageBytes: 0xda000, imageSha256: '74243eaed19fe0c3e5fbb3ef3c46442f71ee8f7c216f214d322894d26ce4e648',
    signature: [0x28f5, 0xa466, 0x0a5a, 0x08e3, 0xc58b],
  },
].map(release => ({ ...release, url: archiveRoot + release.version + '/' + encodeURIComponent(release.archive) }));
