import { expect, test } from 'claude-code/testing'

test('the pane draws the screen and the key row on the terminal', async $ => {
  const ui = await $.ui.mount({
    plugin: 'doom',
    surface: 'terminal',
    component: 'Pane',
    props: { bodyColumns: 100, scroll: { bodyRows: 20 } } as never,
    requestId: 'doom',
    viewport: { columns: 100, rows: 30 },
  })
  expect(await ui.find({ type: 'Raster', key: 'screen' })).toBeDefined()
  expect(await ui.find({ type: 'Client', key: 'keys' })).toBeDefined()
  await ui.unmount()
})
