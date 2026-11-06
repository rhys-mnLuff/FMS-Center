import html from "../../dashboard/forecast.html?raw";
export function GET() { return new Response(html, {headers: {"Content-Type":"text/html; charset=utf-8", "Cache-Control":"no-cache"}}); }
