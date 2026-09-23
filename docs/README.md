# octopass docs

The source of https://octopass.linyo.ws, built with [Next.js](https://nextjs.org/) as a static export.

## Writing

Pages are Markdown files in `content/`:

- `content/index.md` is rendered as the top page (`/`)
- `content/<slug>.md` is rendered as `/<slug>`
- `content/ja/index.md` is rendered as the Japanese top page (`/ja`)

Each file can have `title` and `description` in its front matter.
Static assets such as images go in `public/`.

## Development

```bash
npm install
npm run dev
```

Open http://localhost:3000 in your browser.

## Build

```bash
npm run build
```

The static site is exported to `out/`.
