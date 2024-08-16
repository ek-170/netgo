package server

type Port uint16

func StartTCPServer(port Port) {

}

type Server struct {
  Conns []any // include TCB
}