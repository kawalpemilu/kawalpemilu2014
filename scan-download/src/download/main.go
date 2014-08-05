package main

import (
	"encoding/csv"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"path"
	"strconv"
	"sync"
)

const DOWNLOAD_WORKERS = 4
const LEVEL_KELURAHAN = 4

var imageIndex = 4
var dataDir = "data-c1"

type Data struct {
	pid, tps int
}

func (d *Data) getImageUrl(index int) string {
	return fmt.Sprintf("http://scanc1.kpu.go.id/viewp.php?f=%07d%03d%02d.jpg", d.pid, d.tps, index)
}

func (d *Data) getImageFileName(index int) string {
	return fmt.Sprintf("%s/%04d/%07d%03d%02d.jpg", dataDir, d.pid/1000, d.pid, d.tps, index)
}

func (d *Data) isImageDownloaded(index int) bool {
	fname := d.getImageFileName(index)
	_, err := os.Stat(fname)
	return err == nil
}

func main() {
	if len(os.Args) < 4 {
		log.Fatalf("Usage: %s <lokasi.csv> <image-index> <target-directory>", os.Args[0])
	}

	value, err := strconv.ParseInt(os.Args[2], 10, 32)
	if err != nil {
		log.Fatalf("Invalid image index: %v", os.Args[2])
	}
	imageIndex = int(value)

	dataDir = os.Args[3]

	cdata := readData(os.Args[1])
	cdone := download(filterDownloaded(cdata))
	<-cdone
}

func filterDownloaded(cdata <-chan *Data) <-chan *Data {
	c := make(chan *Data, 1)

	go func() {
		for data := range cdata {
			if !data.isImageDownloaded(imageIndex) {
				c <- data
			}
		}
		close(c)
	}()

	return c
}

func get(url string, client *http.Client, target string) error {
	log.Printf("Download: %s", url)

	req, _ := http.NewRequest("GET", url, nil)
	resp, err := client.Do(req)
	if err != nil {
		return err
	}

	defer resp.Body.Close()

	dir := path.Dir(target)
	os.MkdirAll(dir, 0755)

	tmp := fmt.Sprintf("%s.tmp", target)

	out, err := os.Create(tmp)
	defer out.Close()

	io.Copy(out, resp.Body)

	os.Rename(tmp, target)
	log.Printf("Done: %s", target)

	return nil
}

func download(cdata <-chan *Data) <-chan bool {
	c := make(chan bool, 1)

	go func() {
		var wg sync.WaitGroup
		for i := 0; i < DOWNLOAD_WORKERS; i++ {
			wg.Add(1)

			t := &http.Transport{}
			client := &http.Client{
				Transport: t,
			}

			go func(client *http.Client) {
				for data := range cdata {
					fname := data.getImageFileName(imageIndex)
					get(data.getImageUrl(imageIndex), client, fname)
				}
				wg.Done()
			}(client)
		}

		wg.Wait()
		close(c)
	}()

	return c
}

func parseInt(str string) int {
	val, _ := strconv.ParseInt(str, 10, 32)
	return int(val)
}

func readData(fname string) <-chan *Data {
	c := make(chan *Data, 1)

	go func() {
		file, err := os.Open(fname)
		if err != nil {
			log.Fatalf("Unable to open lokasi.csv: %s", err)
		}
		defer file.Close()

		r := csv.NewReader(file)
		r.Read()
		for {
			row, err := r.Read()
			if err != nil {
				break
			} else {
				pid := parseInt(row[0])
				level := parseInt(row[2])
				jumlah_tps := parseInt(row[4])
				if level == LEVEL_KELURAHAN {
					for i := 1; i <= jumlah_tps; i++ {
						c <- &Data{
							pid: pid,
							tps: i,
						}
					}
				}
			}
		}
		close(c)

	}()

	return c
}
