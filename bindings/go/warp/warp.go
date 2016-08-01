package warp

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"math/rand"
	"net/http"
	"strconv"
)

type Engine struct {
	convert_url	string
	tokenize_url	string
	tr		*http.Transport
	client		*http.Client
}

func NewEngine(addr string) (*Engine, error) {
	w := &Engine {
		convert_url: fmt.Sprintf("http://%s/convert", addr),
		tokenize_url: fmt.Sprintf("http://%s/tokenize", addr),
		tr:		&http.Transport {
			MaxIdleConnsPerHost:		100,
		},
	}
	w.client = &http.Client {
		Transport: w.tr,
	}

	return w, nil
}


type Token struct {
	Word		string		`json:"word"`
	Stem		string		`json:"stem"`
	Language	string		`json:"language"`
	Positions	[]int64		`json:"positions"`
}

type Converted struct {
	Text		string		`json:"text"`
	Stem		string		`json:"stem"`
}

type Request map[string]string
type TokenizedResult map[string][]Token
type ConvertedResult map[string]Converted

func CreateRequest() Request {
	r := make(map[string]string)
	return Request(r)
}

func (r Request) Insert(key, value string) {
	r[key] = value
}

func (w *Engine) send_request(url string, lr Request, want_stem bool) ([]byte, error) {
	lr_packed, err := json.Marshal(lr)
	if err != nil {
		return nil, fmt.Errorf("cound not marshal lexical request: %+v, error: %v", lr, err)
	}
	lr_body := bytes.NewReader(lr_packed)

	http_request, err := http.NewRequest("POST", url, lr_body)
	if err != nil {
		return nil, fmt.Errorf("cound not create warp http request, url: %s, error: %v", url, err)
	}
	xreq := strconv.Itoa(rand.Int())
	http_request.Header.Set("X-Request", xreq)

	if want_stem {
		q := http_request.URL.Query()
		q.Set("stem", "true")
		http_request.URL.RawQuery = q.Encode()
	}

	resp, err := w.client.Do(http_request)
	if err != nil {
		return nil, fmt.Errorf("could not send warp request, url: %s, error: %v", url, err)
	}
	defer resp.Body.Close()

	body, err := ioutil.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("could not read response body: %v", err)
	}

	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("returned status: %d, body: '%s'", resp.StatusCode, string(body))
	}

	return body, nil
}

func (w *Engine) Convert(lr Request, want_stem bool) (ConvertedResult, error) {
	body, err := w.send_request(w.convert_url, lr, want_stem)
	if err != nil {
		return nil, err
	}

	var res ConvertedResult
	err = json.Unmarshal(body, &res)
	if err != nil {
		return nil, fmt.Errorf("could not unpack warp response: '%s', error: %v", string(body), err)
	}

	return res, nil
}

func (w *Engine) Tokenize(lr Request, want_stem bool) (TokenizedResult, error) {
	body, err := w.send_request(w.tokenize_url, lr, want_stem)
	if err != nil {
		return nil, err
	}

	var res TokenizedResult
	err = json.Unmarshal(body, &res)
	if err != nil {
		return nil, fmt.Errorf("could not unpack warp response: '%s', error: %v", string(body), err)
	}

	return res, nil
}

func (w *Engine) Close() {
}
