package main

import (
	"flag"
	"github.com/reverbrain/warp/bindings/go/warp"
	"log"
	"strings"
)

func main() {
	tokenize := flag.Bool("tokenize", false, "tokenize request")
	convert := flag.Bool("convert", false, "convert request")
	stem := flag.Bool("stem", false, "whether to stem reply or not")
	urls := flag.Bool("urls", false, "whether to return array of all urls found in request [a href, img src]")
	addr := flag.String("warp", "", "warp server address")
	text := flag.String("text", "", "message to process, format: prefix:string")

	flag.Parse()

	if *addr == "" {
		log.Fatalf("You must specify warp server address")
	}
	if *text == "" {
		log.Fatalf("There is no text to process")
	}

	if !*tokenize && !*convert {
		log.Fatalf("Neither tokenization, nor conversion has been requested")
	}

	tt := strings.SplitN(*text, ":", 2)
	if len(tt) != 2 {
		log.Fatalf("Invalid text string, it must be in the following format: prefix:string")
	}

	w, err := warp.NewEngine(*addr)
	if err != nil {
		log.Fatalf("Could not create new warp engine: %v", err)
	}

	r := warp.CreateRequest()
	r.Insert(tt[0], tt[1])
	r.WantStem = *stem
	r.WantUrls = *urls

	dump_urls := func(urls []string) {
		for _, url := range urls {
			log.Printf("    url: %s\n", url)
		}
	};

	if *tokenize {
		ret, err := w.Tokenize(r)
		if err != nil {
			log.Fatalf("Tokenization failed, request: %+v, error: %v", r, err)
		}

		for k, tr := range ret.Result {
			log.Printf("%s:\n", k)
			dump_urls(tr.Urls)

			for _, t := range tr.Tokens {
				log.Printf("    word: %s\n", t.Word)
				if *stem {
					log.Printf("    stem: %s\n", t.Stem)
				}
				log.Printf("    lang: %s\n", t.Language)
				log.Printf("     pos: %v\n\n", t.Positions)
			}
		}
	}

	if *convert {
		ret, err := w.Convert(r)
		if err != nil {
			log.Fatalf("Conversion failed, request: %+v, error: %v", r, err)
		}

		for k, cr := range ret.Result {
			log.Printf("%s:\n", k)
			dump_urls(cr.Urls)

			log.Printf("    text: %s\n", cr.Text)
			if *stem {
				log.Printf("    stem: %s\n\n", cr.Stem)
			}
		}
	}
}
