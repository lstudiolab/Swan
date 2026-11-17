package main

import (
	"encoding/json"
	"fmt"
)

const requests = 100000
const payload = `{"id":42,"workers":128,"enabled":true,"service":"swan","items":[1,2,3,4,5],"meta":{"region":"ap-southeast-1","version":1}}`

type Metadata struct {
	Region string `json:"region"`
	Version int64 `json:"version"`
}
type Request struct {
	ID int64 `json:"id"`
	Workers int64 `json:"workers"`
	Enabled bool `json:"enabled"`
	Service string `json:"service"`
	Items []int64 `json:"items"`
	Meta Metadata `json:"meta"`
}

func main() {
	input := []byte(payload)
	var checksum int64
	var totalBytes int64
	for i := 0; i < requests; i++ {
		var value Request
		if err := json.Unmarshal(input, &value); err != nil { panic(err) }
		if value.ID != 42 || value.Workers != 128 { panic("JSON benchmark value mismatch") }
		output, err := json.Marshal(value)
		if err != nil { panic(err) }
		checksum += value.ID + value.Workers
		totalBytes += int64(len(input) + len(output))
	}
	fmt.Println(checksum)
	fmt.Println(totalBytes)
}
