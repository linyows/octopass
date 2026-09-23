import { Html, Head, Main, NextScript, type DocumentProps } from 'next/document'

export default function Document({ __NEXT_DATA__ }: DocumentProps) {
  const lang = __NEXT_DATA__.page.startsWith('/ja') ? 'ja' : 'en'
  return (
    <Html lang={lang}>
      <Head />
      <body>
        <Main />
        <NextScript />
      </body>
    </Html>
  )
}
