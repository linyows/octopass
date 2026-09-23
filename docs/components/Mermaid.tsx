import { useEffect } from 'react'

// Render <pre class="mermaid"> blocks in the page as diagrams.
export default function Mermaid() {
  useEffect(() => {
    const nodes = document.querySelectorAll<HTMLElement>('pre.mermaid')
    if (nodes.length === 0) return

    const media = window.matchMedia('(prefers-color-scheme: dark)')
    const sources = Array.from(nodes, (node) => node.textContent ?? '')

    const render = async () => {
      const { default: mermaid } = await import('mermaid')
      const style = getComputedStyle(document.documentElement)
      const color = (name: string) => style.getPropertyValue(name).trim()
      const background = color('--background-color')
      const foreground = color('--foreground-color')
      const primary = color('--primary-color')
      const secondary = color('--secondary-color')

      mermaid.initialize({
        startOnLoad: false,
        theme: 'base',
        look: 'classic',
        darkMode: media.matches,
        fontFamily: 'inherit',
        themeVariables: {
          background,
          primaryColor: background,
          primaryTextColor: foreground,
          primaryBorderColor: primary,
          secondaryColor: background,
          tertiaryColor: background,
          lineColor: secondary,
          textColor: foreground,
          edgeLabelBackground: background,
        },
      })
      nodes.forEach((node, i) => {
        node.removeAttribute('data-processed')
        node.textContent = sources[i]
      })
      await mermaid.run({ nodes: Array.from(nodes) })
    }

    render()
    media.addEventListener('change', render)
    return () => media.removeEventListener('change', render)
  }, [])

  return null
}
