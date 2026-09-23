import { Lexend, Zen_Kaku_Gothic_New } from 'next/font/google'
import '@/styles/globals.css'
import type { AppProps } from 'next/app'

const lexend = Lexend({
  weight: ['300', '600'],
  subsets: ['latin'],
  display: 'swap',
})

// Zen Kaku Gothic New has no 600, so 700 is used for bold in Japanese.
const zenKakuGothicNew = Zen_Kaku_Gothic_New({
  weight: ['300', '700'],
  subsets: ['latin'],
  display: 'swap',
  preload: false,
})

export default function App({ Component, pageProps }: AppProps) {
  return (
    <>
      <Component {...pageProps} />
      <style jsx global>{`
        :root {
          --font-sans: ${lexend.style.fontFamily};
          --font-ja: ${zenKakuGothicNew.style.fontFamily};
        }
      `}</style>
    </>
  )
}
