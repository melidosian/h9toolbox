/* The v1.0.38 tune/range validator and TX setup both see the actual
 * frequency, including any repeater shift. Ordinary frequencies resume
 * the original instructions and use the original range tables. */
    .section .text,"ax",@progbits
    .globl ml_33cm_validate
ml_33cm_validate:
    r5 = RADIO_33CM_LOW
    if (r4 < r5) goto validate_stock
    r5 = RADIO_33CM_HIGH
    if (r4 > r5) goto validate_stock
    r5 = RADIO_33CM_STATE
    r5 = [r5+0]
    r6 = RADIO_33CM_MAGIC
    if (r5 != r6) goto validate_stock
    r5 = RADIO_33CM_REGION
    r5 = b[r5+0] (u)
    if (r5 > 1) goto validate_stock
    r0 = 1
    {pc, r6-r4} = [sp++]
validate_stock:
    r5 = 0xc0f0
    r6 = 0x01e5671c
    goto r6

    .globl ml_33cm_tx_range
ml_33cm_tx_range:
    r0 = RADIO_33CM_LOW
    if (r5 < r0) goto tx_stock
    r0 = RADIO_33CM_HIGH
    if (r5 > r0) goto tx_stock
    [sp+24] = r5
    r0 = RADIO_33CM_STATE
    r0 = [r0+0]
    r1 = RADIO_33CM_MAGIC
    if (r0 != r1) goto tx_reject
    r0 = RADIO_33CM_REGION
    r0 = b[r0+0] (u)
    if (r0 > 1) goto tx_reject
    /* Skip the stock 500TX and Ham band checks for this band only.
     * Continue through the normal validator call and original RF setup. */
    r7 = 0x01eb1360  /* stock TX table base, still used by RF setup */
    r2 = 1
    r3 = 0x01e5ae60
    goto r3
tx_reject:
    /* The stock reject path still writes RF register 0x30. Clear the
     * caller's TX-active byte and return before touching the RF chip. */
    r0 = 600
    r4 = 0
    b[r9+r0] = r4
    r0 = 0
    sp += 28
    {pc, r9-r4} = [sp++]
tx_stock:
    r0 = 13600000
    r3 = 0x01e5adc8
    goto r3

    .globl ml_33cm_vfo_entry
ml_33cm_vfo_entry:
    /* r0 holds the keyed frequency. The original unsigned range check
     * compares frequency minus 18 MHz with the 582 MHz span. */
    if (r3 <= r4) goto vfo_save
    r3 = RADIO_33CM_LOW
    if (r0 < r3) goto vfo_reject
    r3 = RADIO_33CM_HIGH
    if (r0 > r3) goto vfo_reject
    r3 = RADIO_33CM_STATE
    r3 = [r3+0]
    r4 = RADIO_33CM_MAGIC
    if (r3 != r4) goto vfo_reject
    r3 = RADIO_33CM_REGION
    r3 = b[r3+0] (u)
    if (r3 > 1) goto vfo_reject
vfo_save:
    r4 = 0x01e7dfde
    goto r4
vfo_reject:
    r4 = 0x01e7e00a
    goto r4
