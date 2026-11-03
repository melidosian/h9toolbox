import test from 'node:test';
import assert from 'node:assert/strict';
import { planTransmit, suggestedDirection } from '../dist/channel-plan.mjs';

test('simplex RX changes keep TX in step', () => {
  assert.deepEqual(planTransmit('146.52000', {autoDirection:true}).tx, '146.52000');
  assert.equal(suggestedDirection(146.52), 0);
});

test('US repeater output ranges suggest direction and standard TX', () => {
  assert.deepEqual(planTransmit('146.76000', {autoDirection:true}).tx, '146.16000');
  assert.equal(planTransmit('146.76000', {autoDirection:true}).direction, 1);
  assert.deepEqual(planTransmit('147.12000', {autoDirection:true}).tx, '147.72000');
  assert.equal(planTransmit('147.12000', {autoDirection:true}).direction, 2);
  assert.deepEqual(planTransmit('224.50000', {autoDirection:true}).tx, '222.90000');
});

test('70 cm keeps direction local but applies a selected 5 MHz split', () => {
  const unknown = planTransmit('444.50000', {autoDirection:true});
  assert.equal(unknown.tx, '444.50000');
  assert.match(unknown.message, /varies locally/);
  assert.equal(planTransmit('444.50000', {direction:2}).tx, '449.50000');
  assert.equal(planTransmit('447.50000', {direction:1}).tx, '442.50000');
  assert.equal(planTransmit('447.50000', {direction:2}).tx, null);
});

test('existing custom splits follow RX without being replaced', () => {
  const result = planTransmit('146.95000', {direction:0, customShift:0.475});
  assert.equal(result.tx, '147.42500');
  assert.equal(result.direction, 0);
  assert.equal(planTransmit('146.', {autoDirection:true}), null);
});
