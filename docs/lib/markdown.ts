import fs from 'node:fs'
import path from 'node:path'
import matter from 'gray-matter'
import { unified } from 'unified'
import remarkParse from 'remark-parse'
import remarkGfm from 'remark-gfm'
import remarkRehype from 'remark-rehype'
import rehypeSlug from 'rehype-slug'
import rehypeAutolinkHeadings from 'rehype-autolink-headings'
import rehypeShiki from '@shikijs/rehype'
import rehypeStringify from 'rehype-stringify'
import { visit } from 'unist-util-visit'
import { toString } from 'hast-util-to-string'
import type { Root } from 'hast'

const contentDir = path.join(process.cwd(), 'content')

export type Doc = {
  slug: string
  title: string
  description: string
  html: string
}

// Turn ```mermaid code blocks into <pre class="mermaid"> so that they are
// rendered as diagrams in the browser instead of being highlighted.
function rehypeMermaid() {
  return (tree: Root) => {
    visit(tree, 'element', (node) => {
      if (node.tagName !== 'pre') return
      const code = node.children[0]
      if (code?.type !== 'element' || code.tagName !== 'code') return
      const classes = code.properties.className
      if (!Array.isArray(classes) || !classes.includes('language-mermaid')) return
      node.properties = { className: ['mermaid'] }
      node.children = [{ type: 'text', value: toString(code) }]
    })
  }
}

export function getSlugs(): string[] {
  return fs
    .readdirSync(contentDir)
    .filter((f) => f.endsWith('.md'))
    .map((f) => f.replace(/\.md$/, ''))
    .filter((slug) => slug !== 'index')
}

export async function getDoc(slug: string): Promise<Doc> {
  const file = fs.readFileSync(path.join(contentDir, `${slug}.md`), 'utf8')
  const { data, content } = matter(file)

  const html = await unified()
    .use(remarkParse)
    .use(remarkGfm)
    .use(remarkRehype)
    .use(rehypeMermaid)
    .use(rehypeSlug)
    .use(rehypeAutolinkHeadings, { behavior: 'wrap' })
    .use(rehypeShiki, {
      themes: { light: 'github-light', dark: 'github-dark' },
      defaultColor: false,
      fallbackLanguage: 'text',
    })
    .use(rehypeStringify)
    .process(content)

  return {
    slug,
    title: data.title ?? slug,
    description: data.description ?? '',
    html: String(html),
  }
}
