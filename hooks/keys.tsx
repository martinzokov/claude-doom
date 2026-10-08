import type { ClientModule, JsonValue } from 'claude-code'

type Sent = { n: number; recent: [number, string][] }

// The row under the screen: click it and it takes the keyboard, posting each
// key to the hooks module, which hands it to the engine. Each post carries the
// last few keys by number, so a post replaced by a later one loses nothing.
const Keys: ClientModule<JsonValue, Sent> = (_props, surface) => {
  let sent = surface.state
  if (!sent) {
    const mine: Sent = { n: 0, recent: [] }
    sent = mine
    surface.setState(mine)
    surface.onKey(e => {
      if (e.ctrl || e.meta) return
      const name = e.key === ' ' ? 'space' : e.key
      if (!/^[\x21-\x7e]{1,31}$/.test(name)) return
      mine.n++
      mine.recent = [...mine.recent, [mine.n, name] as [number, string]].slice(-16)
      surface.post({ recent: mine.recent })
    })
  }
  const { Text } = surface.elements

  return (
    <Text dimColor>
      Click here to play (Esc lets go) · WASD/arrows move · space fire · f use/open · q/e strafe · 1-7 weapons · m menu · return select · y yes
    </Text>
  )
}

export default Keys
