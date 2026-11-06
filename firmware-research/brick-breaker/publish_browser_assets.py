"""Publish the exact tested Brick Breaker package and a stock backup seed.

The browser verifies both outputs by SHA-256 before it will inspect or write.
The seed contains only the five original stock sectors changed by the game.
"""

from hashlib import sha256
from pathlib import Path
from shutil import copyfile
import sys

ROOT = Path(__file__).resolve().parent
SITE = ROOT.parents[1] / "dist" / "firmware-update"
sys.path.insert(0, str(ROOT / "tools" / "h9-firmware-research"))
from firmware import parse_firmware  # noqa: E402

STOCK_PACKAGE = ROOT / "input" / "TD-H9-V1.0.38.fw"
GAME_PACKAGE = ROOT / "build" / "h9_brick_breaker_hotkey_OFFLINE_EXPERIMENT.fw"
STOCK_HASH = "c4d0beb5281f3a06d7a56083e1744b925f78ff9917c8364e006418ff0bbd0cd6"
GAME_HASH = "208409d8d7e335904f70f9e28e27e9c413c6296a3d185a69a55707ac8ddaee68"
STOCK_IMAGE_HASH = "74243eaed19fe0c3e5fbb3ef3c46442f71ee8f7c216f214d322894d26ce4e648"
GAME_IMAGE_HASH = "741bbeb87eb84cd7c6988054c56a267bb73c89bbe5cc74dc4e5bc0d75ce7e193"
SECTORS = (0x5000, 0x8D000, 0xAC000, 0xAE000, 0xD9000)
SECTOR_SIZE = 4096


def checked_bytes(path: Path, expected: str) -> bytes:
    data = path.read_bytes()
    if sha256(data).hexdigest() != expected:
        raise ValueError(f"Unexpected SHA-256: {path}")
    return data


def main() -> None:
    stock_bytes = checked_bytes(STOCK_PACKAGE, STOCK_HASH)
    game_bytes = checked_bytes(GAME_PACKAGE, GAME_HASH)
    stock = parse_firmware(STOCK_PACKAGE).image
    game = parse_firmware(GAME_PACKAGE).image
    if sha256(stock).hexdigest() != STOCK_IMAGE_HASH or sha256(game).hexdigest() != GAME_IMAGE_HASH:
        raise ValueError("Embedded image SHA-256 mismatch")
    if stock[:0x5000] != game[:0x5000]:
        raise ValueError("Protected boot bytes differ")
    actual = tuple(offset for offset in range(0x5000, len(stock), SECTOR_SIZE)
                   if stock[offset:offset + SECTOR_SIZE] != game[offset:offset + SECTOR_SIZE])
    if actual != SECTORS:
        raise ValueError(f"Unexpected changed sectors: {actual}")

    seed = b"".join(stock[offset:offset + SECTOR_SIZE] for offset in SECTORS)
    reconstructed = bytearray(game)
    for index, offset in enumerate(SECTORS):
        reconstructed[offset:offset + SECTOR_SIZE] = seed[index * SECTOR_SIZE:(index + 1) * SECTOR_SIZE]
    if bytes(reconstructed) != stock:
        raise ValueError("Stock seed does not exactly reconstruct stock")

    SITE.mkdir(parents=True, exist_ok=True)
    copyfile(GAME_PACKAGE, SITE / "brick-breaker-v1.0.38.fw")
    (SITE / "brick-breaker-stock-sectors.bin").write_bytes(seed)
    print(f"Brick Breaker package: {len(game_bytes)} bytes, SHA-256 {GAME_HASH}")
    print(f"Stock seed: {len(seed)} bytes, SHA-256 {sha256(seed).hexdigest()}")


if __name__ == "__main__":
    main()
