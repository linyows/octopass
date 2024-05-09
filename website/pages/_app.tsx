import { Inter } from 'next/font/google'
import 'rotion/style.css'
import '@/styles/globals.css'
import type { AppProps } from 'next/app'

const inter = Inter({
  weight: ['400', '700'],
  subsets: ['latin'],
  display: 'swap',
})

export default function App({ Component, pageProps }: AppProps) {
  return (
    <>
      <Component {...pageProps} />
      <style jsx global>{`
        :root {
          --font-inter: ${inter.style.fontFamily};
          --rotion-font-family: ${inter.style.fontFamily};
        }
      `}</style>
    </>
  )
}
