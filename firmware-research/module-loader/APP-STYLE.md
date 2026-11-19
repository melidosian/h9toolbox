# H9 Toolbox app style

Follow the Toolbox menu on the 160 × 128 screen. Put the page name in a
small yellow title at the upper left. Reserve the right side for a useful
value such as the current time, page count or fix status. Use steady rows,
short labels and a bright selection band. Keep the few keys the operator
needs at the bottom.

## Palette

Use the RGB565 roles in [app_theme.h](modules/common/app_theme.h). Edit
the palette in that header, then build and install the matching dev
firmware and app bundle.

| Role | Default | Use |
| --- | --- | --- |
| Background and text | Black and white | Reading, measurements and values |
| Accent | Yellow | Titles, selected rows, active tabs and conditions that need attention |
| Edit | White | The value the arrow keys are changing |
| Hint | Cyan | Key prompts and contextual help |
| Dim and rule | Gray | Labels, dividers and secondary information |
| OK and danger | Green and red | Confirmed status and faults or explicit transmit cues |

Use dark text on a bright selection or edit band. Pair a status color
with words or a symbol. Plots and games can use colors that carry meaning
for their own data and pieces.

## Layout and keys

- Use the built-in 5 × 7 font at 1× for labels and controls. A 3- or 4-pixel
  margin leaves 26 character cells. Reserve 2× text for a prominent
  reading, such as the GPS clock.
- Use `ui_title` and `ui_footer` for ordinary pages. Charts and multi-page
  views can place titles, readings and tabs where their data fits. Leave
  space between the plot, its legend and the keys.
- Put field names on the left and short values against a common right edge.
  Keep lists in 12–14 pixel rows where space allows. Make the focused row
  yellow and the edit row white. Show the active tab with the same accent.
- Up and Down move or adjust. Menu opens a tool, picks an item or ends an
  edit. Back returns one level. Display the actual key for each action,
  especially confirmation or cancellation of a transmission.
- Keep the code with `gfx_fill` and `gfx_text` plus the two shared chrome
  helpers. Check the display with empty, full and long-value cases, then
  verify the module still fits below its shared-state boundary.

## APRS

Open **APRS → Stations** for the packet browser. Its keypad tabs open
Map, Messages and Send. Beacon Status is also available directly under
APRS. Put scheduler and receive options in the **APRS Settings** rows.
Keep the packet monitor, raw dumps and audio probes in **Diagnostics**.
