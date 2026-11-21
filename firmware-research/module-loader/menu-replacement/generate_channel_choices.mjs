import {writeFileSync} from 'node:fs';
import {CTCSS_TONES,DCS_CODES} from '../../../dist/channel.mjs';
const bcd = n => Number.parseInt(String(n),16);
const tones = [[65535,'OFF'],...CTCSS_TONES.map(n=>[bcd(Math.round(n*10)),`${n.toFixed(1)} Hz`]),
  ...DCS_CODES.flatMap(s=>[[0x8000|bcd(s),`D${s}N`],[0xc000|bcd(s),`D${s}R`]])];
const lines = ['/* Generated from dist/channel.mjs. */','#ifndef RADIO_TONES_H','#define RADIO_TONES_H',
  'static const struct radio_choice radio_tones[] = {',...tones.map(([v,s])=>`    {${v}, ${JSON.stringify(s)}},`),
  '};','#endif',''];
writeFileSync(new URL('./radio_tones.h',import.meta.url),lines.join('\n'));
