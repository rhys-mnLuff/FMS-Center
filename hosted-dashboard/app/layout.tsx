import type { Metadata } from "next";
import "./globals.css";

export const metadata: Metadata = {
  title: "FMS Live Field Console",
  description: "Live field sensor readings, freshness and flood monitoring.",
  icons: {
    icon: "/favicon.svg",
    shortcut: "/favicon.svg",
  },
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="en">
      <body className="antialiased">{children}</body>
    </html>
  );
}
