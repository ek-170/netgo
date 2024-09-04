# netgo

### 機能

#### Server機能

- 指定したポートで接続を待ち受ける
- TCP/UDPで接続可能
- 受信内容を標準出力に表示

#### Clinet機能

- 指定したホスト/ポートにパケットを送信
- TCP/UDPで送信可能

### TCP Header

       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |          Source Port          |       Destination Port        |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                        Sequence Number                        |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                    Acknowledgment Number                      |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |  Data |       |C|E|U|A|P|R|S|F|                               |
      | Offset| Rsrvd |W|C|R|C|S|S|Y|I|            Window             |
      |       |       |R|E|G|K|H|T|N|N|                               |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |           Checksum            |         Urgent Pointer        |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                           [Options]                           |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                                                               :
      :                             Data                              :
      :                                                               |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


### 参考リンク

- [RFC 9293](https://tex2e.github.io/rfc-translater/html/rfc9293.html#2-2--Key-TCP-Concepts)
- [RFC 4413](https://tex2e.github.io/rfc-translater/html/rfc4413.html)
- [TCP/IP チェックサムの仕組み](https://alpha-netzilla.blogspot.com/2011/08/network.html)
- [The Pseudo-Header in TCP](https://www.baeldung.com/cs/pseudo-header-tcp)
- [PF_PACKETの仕組み](https://enakai00.hatenablog.com/entry/20120522/1337650181)
- [KLab Expert Camp](https://drive.google.com/drive/folders/1k2vymbC3vUk5CTJbay4LLEdZ9HemIpZe)
