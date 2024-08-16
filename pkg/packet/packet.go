package packet

type (
	// TCP Header
	TCPHeader struct {
	 SourcePort uint16
	 DestinationPort uint16
	 SequenceNumber uint32
	 AcknowledgmentNumber uint32
	 DataOffset uint8 // 4bit
	 Reserved uint8 // 4bit, need to 0
	 Window uint16
	 Checksum uint16
	 UrgentPointer uint16
	 Options []Options
 
	 // controll bit
	 CWR byte
	 ECE byte
	 URG byte
	 ACK byte
	 PSH byte
	 RST byte
	 SYN byte
	 FIN byte
 
	 // pseudo-header
	 SourceAddr uint32
	 DestAddr uint32
	 Zero uint8
	 PTCL uint8 // TCPの場合値は6
	 TCPLength uint16
  }

	Options struct {
		Kind Kind
		// TCP implementations MUST be prepared to handle an illegal option length (e.g., zero); a suggested procedure is to reset the connection and log the error cause (MUST-7).
		OptionLength uint8 // 4byte

	}

	Kind uint8

)

const (
	EndOfOptionList = iota
	NoOperationOption
	MaximumSegmentSizeOption
)
