import type { EngineInterface, Register } from 'claude-code'

// Real DOOM: doomgeneric built for this machine (engine/build.sh) running
// Freedoom's IWAD as a child process. It writes each frame to stdout as a
// Raster's cells, which we blit into the pane; keys go back through a file
// it polls (see engine/doomgeneric_term.c).

const PANE = 'doom'
// DOOM's 320x200 is 1.6:1; a cell is two pixels tall, so 3.2 columns a row.
const ASPECT = 3.2

// Black spaces, for before a frame of the pane's size has arrived.
const blank = (cols: number, rows: number) => {
  const words = new Uint32Array(cols * rows * 3).map((_, i) => (i % 3 === 0 ? 0x20 : 0))
  const bytes = new Uint8Array(words.buffer)
  let text = ''
  for (let i = 0; i < bytes.length; i += 8192) text += String.fromCharCode(...bytes.subarray(i, i + 8192))
  return btoa(text)
}

// The largest screen of DOOM's shape that fits the pane, under the key row.
const fit = (columns: number, bodyRows: number) => {
  const rows = Math.max(4, Math.min(bodyRows - 1, Math.floor(columns / ASPECT), 120))
  return { cols: Math.max(8, Math.min(columns, Math.round(rows * ASPECT))), rows }
}

let game: AsyncGenerator<unknown, unknown> | undefined
let frame: { size: string; cells: string } | undefined
let size = { cols: 80, rows: 25 }
let sentSize = ''
let seq = 0
let lastKey = 0
let events: string[] = []

const paths = ($: EngineInterface) => {
  const engine = `${$.plugin.root}/engine`
  return {
    build: `${engine}/build.sh`,
    bin: `${engine}/bin/doom`,
    wad: `${engine}/bin/freedoom1.wad`,
    keys: `${engine}/bin/keys`,
  }
}

const send = async ($: EngineInterface, names: string[]) => {
  for (const name of names) events.push(`${++seq} ${name}`)
  events = events.slice(-48)
  await $.fs.write(paths($).keys, events.join('\n') + '\n')
}

const start = async ($: EngineInterface) => {
  if (game) return
  const p = paths($)
  seq = 0
  lastKey = 0
  events = []
  frame = undefined
  sentSize = ''
  await $.fs.write(p.keys, '')
  const child = $.process.spawn({
    argv: [p.bin, '-iwad', p.wad, '-skill', '3', '-warp', '1', '1'],
    cwd: `${$.plugin.root}/engine/bin`,
    env: { DOOM_KEYS: p.keys, DOOM_COLS: String(size.cols), DOOM_ROWS: String(size.rows) },
  })
  game = child
  void (async () => {
    let pending = ''
    try {
      for await (const { stream, text } of child) {
        if (stream !== 'stdout') continue
        pending += text
        const lines = pending.split('\n')
        pending = lines.pop() ?? ''
        const last = lines.findLast(line => line.startsWith('F '))
        if (!last) continue
        const [, dims = '', cells = ''] = last.split(' ')
        frame = { size: dims, cells }
        if (dims !== `${size.cols}x${size.rows}`) continue
        await $.ui.blit({ requestId: PANE, key: 'screen', cells }).catch(() => undefined)
      }
    } finally {
      if (game === child) game = undefined
    }
  })()
}

const stop = async ($: EngineInterface) => {
  const child = game
  if (!child) return
  game = undefined
  await send($, ['QUIT']).catch(() => undefined)
  await child.return(undefined)
}

export const register: Register = on => {
  on('session.start', async ($, e, next) => {
    await $.command.register({
      name: 'doom',
      description: 'Play DOOM in a pane',
    })

    return next(e)
  })

  on('command.run', { command: 'doom' }, async $ => {
    const p = paths($)
    if (!(await $.fs.exists(p.bin)) || !(await $.fs.exists(p.wad))) {
      $.ui.toast('Building DOOM for this machine (first run only)...')
      const built = await $.process.run(['sh', p.build], { timeoutMs: 600_000 })
      if (built.exitCode !== 0) {
        return { text: `Could not build DOOM:\n${built.stderr.slice(-2000)}` }
      }
    }
    await start($)
    await $.ui.open({ id: PANE, title: 'DOOM', focus: true, rows: 60, columns: 200 })
    await $.ui.focus({ requestId: PANE, key: 'keys' }).catch(() => undefined)

    return { text: 'DOOM is running. If keys do nothing, click the line above the screen. Esc gives the keys back; close the pane to quit.' }
  })

  on('ui.message', { requestId: PANE }, async ($, e) => {
    const recent = (e.data as { recent?: [number, string][] }).recent ?? []
    const fresh = recent.filter(([n]) => n > lastKey)
    if (fresh.length === 0) return {}
    lastKey = fresh[fresh.length - 1]![0]
    await send($, fresh.map(([, name]) => name))

    return {}
  })

  on('ui.close', { id: PANE }, async ($, e, next) => {
    await stop($)

    return next(e)
  }).catch(($, e, next) => next(e))

  on('session.end', async ($, e, next) => {
    await stop($)

    return next(e)
  })

  on('ui.render', { component: 'Pane', requestId: PANE }, async ($, e) => {
    if (e.surface !== 'terminal') {
      const { Text } = $.ui.resolve(e)
      return <Text dimColor>DOOM plays in the terminal.</Text>
    }
    const { Box, Raster, Client } = $.ui.resolve(e)
    size = fit(e.props.bodyColumns, e.props.scroll.bodyRows)
    const dims = `${size.cols}x${size.rows}`
    if (game && dims !== sentSize) {
      sentSize = dims
      void send($, [`SIZE:${dims}`]).catch(() => undefined)
    }

    return (
      <Box flexDirection="column">
        <Client key="keys" module="./keys.tsx" width={size.cols} height={1} />
        <Raster
          key="screen"
          columns={size.cols}
          rows={size.rows}
          cells={frame?.size === dims ? frame.cells : blank(size.cols, size.rows)}
        />
      </Box>
    )
  })
}
